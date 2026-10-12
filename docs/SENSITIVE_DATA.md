# Sensitive data contract

`domain::SensitiveDataPolicy` is the **security policy abstraction** for generated values.
Generators declare which output fields are sensitive (`OutputFieldSpec::sensitive`); every
component that moves generated values out of the generator must ask the policy what to do.
The policy decides; it performs no I/O, shows no dialogs and wipes nothing.

## Record shape contract (decided before Slice 2)

* **Records are flat.** Every output field holds a **scalar** JSON value (string, number, boolean, null).
  Nested objects and arrays are *not* part of the contract. `domain::validateRecord(metadata, record)`
  checks that every key is declared and every value is a scalar; every generator's test suite must call
  it on real output (the password tests do).
* **Sensitivity applies per top-level key to the whole value.** The policy never recurses: names of nested
  keys are irrelevant, nested keys cannot be declared sensitive individually, and no future generator may
  assume that sanitisation is recursive. A generator that needs partial sensitivity must flatten the data
  into separate declared fields.
* **Fail closed anyway.** If a value that violates the contract (an object or array) reaches the policy, the
  *entire* value, with all its descendants, is treated as sensitive even under a field declared
  non-sensitive: logged as `[REDACTED]`, masked on display, omitted from persistence/workspace, redacted on
  export/clipboard unless explicitly authorised (then released unchanged, as a whole), and it triggers
  `requiresConfirmation`. Tests: `tst_sensitive` (nested objects, arrays, mixed, whole-subtree redaction,
  scalars of every JSON type pass untouched, `validateRecord`).

## Per-channel contract

"Enforced now" means the behaviour exists in code **and** is covered by a test. "Contract only"
means the consumer does not exist yet, so nothing can currently violate or honour the rule.

| Channel | Contract (what any consumer MUST do) | Enforced now | Contract only (consumer not built) | Planned |
|---|---|---|---|---|
| **Log** | Sensitive values are always `[REDACTED]`; not configurable. Error/validation messages must never embed generated values or user-supplied secrets. | `handling()`/`sanitize()` always redact (even with every opt-in on). Validation messages do not echo supplied character sets (test). Errors from the generator contain no password. | There is **no logger yet**; nothing forces future code to call the policy. | Slice 3: structured logger that accepts only policy-sanitised records + an architecture test that bans logging raw `GeneratedRecord`s. |
| **Persistence** (presets, settings, any app-written file) | Sensitive values omitted unless the user explicitly opts in (`persistSensitiveValues`). Presets never contain results at all. | Policy omits sensitive fields by default; opt-in works (tests). | No preset/settings store exists. | Slice 3 (JSON persistence). |
| **Workspace** (saved project state with results) | Same as persistence; default must be "Do not persist generated passwords". | Policy treats `Workspace` like `Persistence` (tests, mutation M14). | No workspace store exists. | Slice 3. |
| **Display** (results view) | Masked with a fixed-width mask (no length leak) until the user reveals. | `Mask` unless `revealOnDisplay`; mask is length-independent (test). | No results view exists. | Slice 4 (GUI). |
| **Export** | Release only after the user explicitly confirmed. Confirmation text must name that the file will contain secrets; restrictive file permissions; atomic write; no temp plaintext. | **At API level:** without `ReleaseAuthorization::userConfirmed(Export)` every sensitive or undeclared field is replaced by `[REDACTED]`; `requiresConfirmation(record, Export)` inspects the actual record. Tests: undeclared extra field, no authorization, authorization, wrong-channel authorization. | The dialog, the file writer, permissions and atomic write do not exist. | Slice 3/4. |
| **Clipboard** | Same authorization rule; offer auto-clear timeout; never store in app history; warn that other apps can read the clipboard. | Same as Export (separate channel token: an Export authorization does not release to the clipboard). | No clipboard adapter, no timeout, no dialog. | Slice 4. |
| **Undeclared fields** | A key not declared in `outputFields` is treated as sensitive (fail closed). | Yes (tests, mutation M15). | - | - |

### The authorization mechanism and its honest limits

`ReleaseAuthorization::none()` (the default argument) never releases anything. Only
`ReleaseAuthorization::userConfirmed(channel)`, written explicitly by the caller and bound to ONE
channel (Export or Clipboard), allows plaintext through `sanitize()`/`handling()`. It cannot unlock
Log, Display, Persistence or Workspace. A scan test restricts the words `userConfirmed(` to the policy
header, `src/gui/` and tests, so non-UI code cannot call it unnoticed.

This is a speed bump that prevents *accidental* release, **not an unforgeable capability**: any
in-process code can still call `userConfirmed`. Whether a real confirmation dialog preceded the call is
a contract on the future UI layer and is not enforced by the library.

Behavioural guarantees that **are** tested: the password generator declares `password` as
sensitive; generated passwords do not survive log or persistence sanitisation in a 50-record
check; a failed generation returns no partial records.

## What is explicitly NOT promised

* **Memory.** Nothing here guarantees that plaintext is erased from memory. `common::secureWipe`
  zeroes only the internal `std::u32string` scratch buffers the generator uses. Generated values
  also exist as `QString`, inside `QJsonObject` records, in a `QSet<QString>` when a batch must be
  unique, in the sink's container, in the UI, and possibly in swap, core dumps or the clipboard.
  `QString::clear()` or destroying a string does **not** zero its buffer, and implicit sharing means
  copies may outlive the original. Treat in-process secrets as recoverable by a local attacker with
  memory access.
* **Process isolation, anti-debugging, or protection against other apps reading the clipboard.**
* **That consumers call the policy.** Until the logger/exporter/persistence layers exist, this is a
  documented rule, not an enforced one. What *is* enforced: if a consumer does call `sanitize()`, an
  unauthorised Export/Clipboard cannot receive a sensitive or undeclared value.

## Rules for new generators

Mark every credential-like column `sensitive`, declare all output columns, and never put secrets
in error messages. Nothing else about the generator needs to know about the policy.
