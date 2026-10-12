# Randomness audit: what Qt's `QRandomGenerator::system()` really does, and what we use instead

## Correction

Earlier revisions of this repository claimed that "Qt aborts the process if the OS entropy source
is unavailable", and `SecureRandomProvider` was a thin wrapper over `QRandomGenerator::system()`.
**That claim was wrong.** Qt's own documentation describes fallbacks, and the implementation
contains them. `SecureRandomProvider` no longer uses any Qt random class (a scan test forbids the
identifier anywhere under `src/`).

## Audit of Qt 6.4.2 (upstream tag `v6.4.2`, `src/corelib/global/qrandom.cpp`)

Source read from `https://raw.githubusercontent.com/qt/qtbase/v6.4.2/src/corelib/global/qrandom.cpp`.
Distribution patches in the Ubuntu package that we compiled against were **not** examined; line
numbers refer to the upstream tag.

| # | Finding | Where |
|---|---|---|
| 1 | The generate path first honours a test hook: when `qt_randomdevice_control` has the `SetRandomData` bit, the whole buffer is filled with one constant value. | l. 288-293 |
| 2 | It then **prefers the CPU hardware RNG** (`qHasHwrng()` / `qRandomCpu`, i.e. RDRAND/RDSEED on x86) and uses the OS only for what is missing. So "system()" is not "the OS CSPRNG" on those CPUs. | l. 295-296 |
| 3 | If the OS call produced fewer bytes than requested, the remainder comes from `fallback_fill`. On builds without `getentropy` that fallback is a **`std::mt19937` seeded from clock/addresses (not cryptographic)**; the generic Unix path reads `/dev/urandom` and a failed read leads to that fallback. | l. 305-309, 170-278 |
| 4 | On builds that use `getentropy`, a failing call is checked **only by `Q_ASSERT(ret == 0)`**, which in this file is redefined to the C `assert` (l. 42-48) and therefore vanishes when `NDEBUG` is defined, as in typical release builds. The function reports the full count as read regardless, and `fallback_fill` for that configuration is an empty function (l. 181-186: "getentropy cannot fail under normal circumstances"). A failure is therefore **neither reported nor replaced**: the caller cannot tell. | l. 67-81, 181-186 |
| 5 | The documentation states that the OS may "fall back to a pseudo-random engine for a time" when entropy is exhausted, that on "other operating systems, QRandomGenerator will fall back to a PRNG of good numeric distribution" without guaranteed seeding, and tells applications that must not fall back to non-cryptographic generators to check OS documentation or restrict deployment. | docs, l. 504-525 |

Empirical observation (Linux x86-64, `tst_secure_random`, informational test): with the
`getrandom(2)` syscall made to fail by a seccomp filter, `QRandomGenerator::system()->generate64()`
**returned normally with no error indication**. This does not show which internal path produced the
value (finding 2 means the hardware RNG may have answered); it does show that a caller has no
signal to act on.

Conclusion: `QRandomGenerator::system()` cannot give this project the property it requires,
**fail-closed OS CSPRNG, with failures reported**. The application requirement is fail-closed, so
Qt's generator is not used at all. No cryptographic algorithm is implemented in this project: we
call the operating system's own CSPRNG interface directly.

## Backends (`src/infrastructure/random/OperatingSystemEntropy.cpp`)

| Platform | Backend | Failure reporting | Behaviour / limits | Verified here |
|---|---|---|---|---|
| Linux (glibc >= 2.25, musl, Android API 28+) | `getrandom(2)`, flags `0` | returns `-1`/`errno`; mapped to `EntropyError{errno, text}` | Blocks until the kernel pool is initialised (so no weak early-boot output). Retries only on `EINTR` (bounded). **No `/dev/urandom` fallback**: kernels without the syscall (`ENOSYS`) yield an error. | **Compiled and tested** (GCC 13, Clang 18); real-syscall fault injection via seccomp |
| macOS / iOS | `getentropy(2)` from `<sys/random.h>`, <= 256 bytes per call | returns `-1`/`errno` | Needs macOS 10.12 / iOS 10 or newer. | **Never compiled** |
| Windows | `BCryptGenRandom(nullptr, ..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)` | `NTSTATUS`, `BCRYPT_SUCCESS` macro | Links `bcrypt`. System-preferred RNG; Windows 7+ semantics per Microsoft documentation. | **Never compiled** |
| anything else | `#error` | n/a | Compilation refuses rather than choosing a weaker source. | n/a |

The macOS and Windows code was written against the documented APIs and reviewed statically only.
It must be compiled and run on those platforms before anyone treats it as verified.

## Guarantees of `SecureRandomProvider`

* Returns bytes from the backend above, or throws `domain::RandomSourceError`. Nothing is substituted.
* `isCryptographicallySecure()` is `true` only for a provider that was **constructed with the
  attestation token** (see "Trust boundary" below). It is no longer derived from anything the entropy
  source or a custom provider can say about itself.
* Generators translate `RandomSourceError` into `GenerationError::RandomSourceFailure`, stop at the
  first failure (no retry, no second source) and return no records through `generate()`.
* `probe()` lets a composition root fail at startup instead of at first use (the CLI demo does).
* One OS call per 64-bit word, no buffering, no state: thread-safe, nothing secret cached.

## Trust boundary (round 3)

How a security-sensitive generator obtains "evidence" that randomness meets the contract:
`IGenerator::checkPreconditions()` reads `context.random().isCryptographicallySecure()`. That is
**not independent evidence about the operating system**; it is an *attestation made at construction
time by trusted code*. The earlier design let any implementation answer a virtual function (and let
an injected `ISystemEntropy` claim `isOperatingSystemSource() == true`); a test even demonstrated a
double doing so. That is closed as follows.

| Mechanism | Effect |
|---|---|
| `IRandomProvider::isCryptographicallySecure()` is **non-virtual** and returns a private flag | A custom provider cannot override it. A derived function of the same name merely hides it; generators call through the base. Tested (`LyingProvider`). |
| The flag is set only by a protected constructor taking a `SecureAttestation&&` | Every provider is untrusted by default (test doubles, `DeterministicRandomProvider`, third-party providers). |
| `SecureAttestation` has a **private** constructor, deleted copy/move, and exactly one friend, `SecureRandomProvider` | Ordinary code cannot create or stash a token. `static_assert`s in `tst_secure_random` fail the build if this weakens. |
| `SecureRandomProvider` mints a token **only in its public default constructor**, which builds the built-in OS source itself | The one attested provider is the production one. |
| The constructor that accepts an injected `ISystemEntropy` is **private** (friend: `testing::EntropyInjectionAccess`, defined in `testsupport/`) | A provider built around a custom/fault-injecting source is **never** attested, whatever the source returns. `ISystemEntropy` lost its `isOperatingSystemSource()`: a source can no longer vouch for itself. `static_assert(!is_constructible_v<SecureRandomProvider, shared_ptr<ISystemEntropy>>)` guards it. |
| `testsupport/` is exported only by `ar_random_testing`; scan tests ban `providerWithInjectedSource`, `EntropyInjectionAccess`, `SecureAttestation` outside their designed files | Production targets cannot compile the injection path. |

Fault injection is unchanged in power: tests run generators against injected failing sources through
`testing::insecureContext()` (an explicitly insecure test context), and one test runs the **real
production path** (attested provider, ordinary context, real `PasswordGenerator`) with the real
`getrandom` syscall failing, expecting `RandomSourceFailure` and no records.

**What this does and does not claim.** The API now prevents *accidental* and *ordinary* use of an
untrusted provider as a secure one: misconfiguration, test doubles, third-party providers. It does
**not** prevent every unsafe provider: code that deliberately subverts the type system (editing the
headers, ODR violations, memory corruption, or a replaced `libar_random`) is out of scope, and the
design still *trusts* three things: the implementation of `SecureRandomProvider`, the OS backend
behind it, and the operating system. Nothing in the program independently verifies that bytes are
random (that cannot be proven by testing).

## Fault-injection evidence (`tst_secure_random`)

* Source double failing at call 0 / 1 / 37: the generator makes exactly `failAfter + 1` calls and
  returns `RandomSourceFailure` (no retry, no fallback).
* Failure mid-batch: `generate()` returns no passwords at all.
* A provider built around any injected source, and any custom provider (including one that hides the
  flag with its own function), is rejected by an ordinary context before any byte is drawn.
* The production path (attested provider + ordinary context + real generator) under a denied
  `getrandom` returns `RandomSourceFailure` and no records (separate-process test).
* **Real syscall:** a helper process installs a seccomp filter that makes `getrandom` fail with
  `EPERM`, then calls the real backend. The backend reports `EPERM`; the test fails if it ever
  reports success (mutation M26 proves the test notices). The test is skipped, not passed, where
  seccomp is unavailable.

## Limits of this audit

* Qt versions other than 6.4.2 were not read; newer Qt may behave differently. This matters little
  now because Qt's generator is not used, but the scan test keeps it that way.
* The OS itself is trusted (kernel CSPRNG, hardware, hypervisor). Virtual machine snapshot/restore
  and early-boot entropy are outside what this project can verify.
