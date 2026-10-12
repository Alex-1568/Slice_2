# Public API audit (before Slice 2)

Stability policy: **every contract below is "experimental"** until Slice 2 has been built on it.
Any change that forces third-party generators to be rebuilt must bump
`domain::kGeneratorApiVersion`. Adding a pure-virtual function to an interface is such a change;
new virtuals must come with a default implementation.

## Per-contract review

| Contract | Ownership / lifetime | Thread safety | Exceptions | Cancellation | Invalid input | Qt types appropriate? | Size |
|---|---|---|---|---|---|---|---|
| `IRandomProvider` | `isCryptographicallySecure()` is **non-virtual** and set only through a `SecureAttestation` token that only `SecureRandomProvider` can create; every other provider is untrusted by default. Caller owns the provider; it must outlive every `GenerationContext` that references it. Copy is protected (no slicing). | Per implementation: `SecureRandomProvider` is stateless and thread-safe when its `ISystemEntropy` is (the OS source is); `DeterministicRandomProvider` is **not** (one per worker). | `nextUInt64` may throw `RandomSourceError`; `uniformIndex(0)` throws `std::invalid_argument`; broken source -> `RandomSourceError` after 256 rejections (bounded). | n/a | Contract violation = exception, never UB. | No Qt types. | 2 virtuals + 2 non-virtual helpers. |
| `CancellationToken/Source` | Token = shared view of the source's flag (`shared_ptr<atomic<bool>>`); outlives the source safely. | Fully thread-safe, lock-free, `noexcept`. | none | The mechanism itself. Per-source flag; no global state. | n/a | none | 2 tiny classes. |
| `GenerationContext` | Non-owning pointer to the provider; copyable value; make one per worker. | Immutable after construction; safe to share by const reference. | none | Exposes `isCancelled()`. | **No public way to accept a non-CSPRNG provider.** The switch is private; the only access is `testing::insecureContext()` in `testsupport/`, whose include path is exported solely by `ar_random_testing` (production targets cannot compile a call to it). Circumvention by deliberately declaring a friend-named struct stays possible for in-process code; the boundary targets accidents. | `ApplicationLimits` is plain `size_t`. | 4 accessors. |
| `IGenerator` | Registry holds `shared_ptr<const IGenerator>`; callers keep a pointer alive for the duration of a run. | All methods `const`; implementations must be stateless/immutable so one instance serves concurrent workers (verified for `PasswordGenerator`). | Basic guarantee. `RandomSourceError` is translated to an error `Result`. Other exceptions (e.g. from a throwing sink) propagate; `generate()` then returns nothing, so partial secrets are not handed out. | Checked between records; reported as `CompletionStatus::Cancelled` (a value, not an error); records produced so far are delivered. | `validate()` + `checkPreconditions()`; invalid requests never reach generation logic. | `QString`/`QJsonObject`/`QList` are the domain vocabulary (decision in ADR). | 3 virtuals + 1 non-virtual convenience. |
| `IResultSink` | Borrowed for one call. | Called on the generating thread; the sink synchronises its own state. | Must not throw (documented). | Sink may request early stop (`SinkResponse::Stop`). | n/a | `QJsonObject` record. | 1 virtual. |
| `GeneratorRegistry` | Owns generators via `shared_ptr`; `find()` returns shared ownership so unregistering mid-run is safe (tested). | `shared_mutex`: concurrent readers, exclusive writers (tests + TSan `concurrency_check`). | Registration failures are `Result` errors, not exceptions. | n/a | Duplicate id, invalid id/version/schema/outputs -> typed `RegistryError`. | `QString` ids. | 6 functions. |
| `GeneratorRequest` | Plain value. | Value type. | none | n/a | `count < 1`, `count > maxBatchSize`, unsupported `unique` -> `GenerationError`. **Hazard:** an *empty* `parameters` object means "use defaults"; a caller that turns a failed JSON parse into an empty object would silently generate with defaults. Persistence code must check `QJsonParseError` (tested that malformed text is detectable). | `QJsonObject` is the extensible parameter bag. | 4 fields. |
| `GeneratorResult` / `GenerationSummary` | Plain values. | Value types. | none | `status` carries Cancelled/StoppedBySink. | n/a | `QList<QJsonObject>`: fine for Slice 1; large batches should use the sink. | small |
| `GenerationError` | Plain value; **never contains generated secrets**. | Value type. | none | n/a | Carries a `ValidationResult` for `InvalidConfiguration`. | `QString` message. | enum + 3 fields |
| `ConfigurationSchema` / `FieldSpec` | Value types, copied into metadata. | Immutable after construction. | none | n/a | `validate()` is total: wrong types, ranges, unknown keys, too many keys (`maxParameterCount`) and over-long strings (`maxStringLength`) are reported, never thrown. Limits can only *tighten* a field. | `QJsonValue` default values. | 4 field types only (deliberately not JSON Schema). |
| `SensitiveDataPolicy` + `ReleaseAuthorization` | Value types; no I/O. | Immutable; safe to share. | none | n/a | Undeclared keys are sensitive; **records are flat (scalar values)** and an object/array value is treated as sensitive as a whole (no recursion, see `docs/SENSITIVE_DATA.md`). Export/Clipboard release a sensitive/undeclared value only with a matching `ReleaseAuthorization`; confirmation is decided from the **actual record** (`requiresConfirmation(record, channel)`). The authorization is a speed bump, not an unforgeable capability. | `QJsonObject` records. | see `docs/SENSITIVE_DATA.md` |
| `ISystemEntropy` / `SecureRandomProvider` | Provider owns (shared) its source. | Source must be thread-safe; the OS source is. | `fill()` returns a `Result`; the provider converts failure to `RandomSourceError`. | n/a | Zero-length fill is a no-op. The source-injecting constructor is **private** (friend `testing::EntropyInjectionAccess` in `testsupport/`) and yields a never-attested provider; null source throws `std::invalid_argument`. | `QString` only in the error text. | 2 virtuals (`fill`, `backendName`); a source can no longer claim to be the OS. |
| `IGeneratorPlugin` | Boundary only; **no loader exists**. | `registerPluginGenerators` is not synchronised: call at startup. | Escaping exceptions are converted to errors. | n/a | Wrong `apiVersion()` rejected before registration. | `QString`. | 4 virtuals + 1 free function. |

## Findings from this audit and what was done

| # | Finding | Action in 1.1 |
|---|---|---|
| F1 | `std::stop_token` is not available in libc++ 18 without `-fexperimental-library` (verified by compiling), so Apple/LLVM builds were at risk. | Replaced by the project-owned `CancellationToken/Source`. |
| F2 | `validate()` could not see runtime limits, and parameter-count / string-length ceilings did not exist. | `ConfigurationSchema::validate(params, limits)`, `maxParameterCount`, `maxOutputLength`; `validate()` applies the defaults, `generateInto()` applies the context's limits. `IGenerator::validate` keeps its signature on purpose (see "Deferred"). |
| F3 | `DeterministicRandomProvider` shipped inside the production RNG library. | Own library `ar_random_testing`; production targets link only `ar_random`; a source-scan test enforces it for `tools/` and `app/`. |
| F4 | `entropyBits`/`strength` conflated a configuration's search-space bound with password strength. | Replaced by `guaranteedEntropyBits` (lower bound), `searchSpaceBits` (upper bound), `entropyQuality`; no `strength` field; band derived from the lower bound only. |
| F5 | No plugin abstraction existed although the roadmap promised one. | `IGeneratorPlugin` + `kGeneratorApiVersion` + `registerPluginGenerators` (in-process test doubles only). |
| F6 | Exception behaviour of sinks undocumented. | Documented above. |
| F7 | Empty-parameters-means-defaults hazard. | Documented; test shows malformed JSON is detectable. |
| F8 | **(round 2)** `SecureRandomProvider` wrapped Qt's system generator, whose real behaviour includes hardware-RNG preference, silent fallback and ignored OS failure. | Replaced by fail-closed OS backends behind `ISystemEntropy`; Qt random classes banned under `src/`; fault injection incl. a real seccomp-denied `getrandom` (`docs/RANDOMNESS_AUDIT.md`). |
| F9 | **(round 2)** `requiresConfirmation(channel)` looked only at declared metadata and `handling()` let sensitive values through Export/Clipboard on the caller's honour. | Record-level confirmation decision; `ReleaseAuthorization` bound to a channel; default is redaction; four regression scenarios. |
| F10 | **(round 2)** `GenerationContext::forTesting()` lived in the production header. | Removed; private switch + `testsupport/` helper not visible to production targets. |
| F12 | **(round 3)** `isCryptographicallySecure()` was a virtual self-attestation; the public injection constructor let a test double make `SecureRandomProvider` claim security. | Non-virtual flag + construction-time `SecureAttestation` token (private ctor, one friend); injection constructor private behind `testsupport/`; `isOperatingSystemSource()` removed; build-time `static_assert`s; limits stated in `docs/RANDOMNESS_AUDIT.md`. |
| F13 | **(round 3)** Nested JSON in records: sanitisation depth unspecified. | Flat-record contract + whole-value handling + fail-closed treatment of objects/arrays; `validateRecord`; documented in `docs/SENSITIVE_DATA.md`. |
| F14 | **(round 3)** TSan class `KNOWN-QTTEST` accepted the watchdog name anywhere in a report. | The watchdog must be one of the threads that performed the racing accesses, in a plain data race; creation annotations do not count. New negative self-tests; the checker's self-test is now a ctest entry under mutation testing. |
| F11 | **(round 2)** `verify.sh` ignored TSan failures (`|| true`). | `tools/check_tsan.py` + exit-status plumbing + `selftest` proving failures are detected. |

## Deferred / consciously not changed

* **Qt types in public interfaces** couple future plugins to the exact Qt major/minor and compiler
  ABI of the host. `apiVersion()` does not capture that. A future loader must also compare the Qt
  version (and likely the compiler/stdlib) before loading. Replacing Qt types with `std::` types
  would add conversions everywhere; not done without a concrete need.
* **`IGenerator::validate` has no `limits` parameter.** Adding one (or a default argument on a
  virtual) was rejected as speculative; the context-aware checks run in `generateInto()` instead.
* **Unicode tables depend on the Qt version.** `QChar::category` and NFC normalisation come from Qt;
  a custom-character set that is valid under one Qt version could (rarely) be classified differently
  under another. Presets shared across Qt versions inherit that risk.
* No abstraction was added "for the future".
