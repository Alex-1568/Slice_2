# Slice 1 / 1.1 — design notes

Status: Slice 1 architecture approved; **Slice 1.1 (hardening and verification) is the current
state**. Slice 2 has not been started. Evidence for every claim of "tested" lives in
`docs/VERIFICATION.md`; this file does not repeat numbers.

## Corrections to the first Slice 1 report

| Earlier statement | Accurate statement |
|---|---|
| "11 mutations were caught" | **10** mutations were run and caught. An 11th attempt never applied (its search text did not match) and was not counted in the results. The mutation experiment was also not reproducible from the archive. Both are fixed in 1.1: see `tools/mutation/` and `docs/mutation-report.md`. |
| TSan summarised as passing | Qt Test suites under TSan **do** produce reports. They originate in Qt Test / system-library teardown, with no project-code frames in the observed stacks. A dedicated non-Qt-Test executable (`concurrency_check`) is used for a clean-signal TSan run. See `docs/VERIFICATION.md`. |
| `entropyBits` / `strength` in records | Replaced (see "Entropy semantics"). |
| "Library is portable" (implicit) | `std::stop_token` would have failed on libc++ toolchains; replaced. Platform status in `docs/PORTABILITY.md`. |

## Data flow

```
caller ──> GeneratorRegistry.find("password") ──> shared_ptr<const IGenerator>
caller ──> GenerationContext{ IRandomProvider&, CancellationToken, ApplicationLimits }
IGenerator::generate(request, ctx)            -- convenience, collects in memory
   └─> generateInto(request, ctx, IResultSink&)   -- streaming primitive
         ├─ checkPreconditions (count, batch limit, unique support, secure-RNG policy)
         ├─ prepare(request, ctx.limits): schema -> parse -> pool plan -> semantic validation -> entropy
         └─ per record: cancel check -> generatePasswordCodePoints -> sink.accept
SensitiveDataPolicy.sanitize(record, channel)  -- see docs/SENSITIVE_DATA.md
```

Library layering (enforced by CMake and a source-scan test):
`ar_generators -> ar_core`; `ar_random -> ar_core` (production RNG);
`ar_random_testing -> ar_core` (deterministic RNG and the insecure-context helper, tests/benchmarks only, exports `testsupport/`).

## Entropy semantics (the 1.1 priority fix)

The generator reports **three** numbers, all about the *configuration*, none about a particular
password:

| Field | Meaning |
|---|---|
| `guaranteedEntropyBits` | **Lower bound** on the generator's min-entropy (so also on Shannon entropy and guessing resistance), assuming an ideal RNG and an attacker who knows the settings. |
| `searchSpaceBits` | **Upper bound**: log2 of the number of strings the unconstrained pool allows. Entropy can never exceed it. Never present it as "strength". |
| `entropyQuality` | `exact` if the bounds coincide (no active constraint that removes outputs), otherwise `bounded`. |

Why a lower bound is rigorous: the draw sequence before the shuffle is uniform over its valid
values, and the shuffle is an independent uniform permutation, so every output string has
probability at most `2^-guaranteedEntropyBits`. (dup-allowed: `Σ min_c·log2 n_c + (L−M)·log2 N`;
no-duplicates: the same with falling factorials.) This was checked against the **exactly
enumerated** output distribution of a tiny configuration and against 200 000 real samples
(`tst_entropy_semantics`). In that example the true min-entropy and Shannon entropy sit strictly
inside the interval, i.e. quoting the upper bound as "the entropy" would have been wrong.

Bands (`EntropyBand`: weak … very-strong) are derived from the **lower** bound only. A per-password
verdict (patterns, repetition, dictionary words) would be a separate `PasswordStrengthAssessment`
that takes the password as input; it is deliberately not implemented and the name is not used.

Caveat: the figures assume the provider is ideal. They say nothing about a compromised or
low-entropy OS source, nor about guessing attacks that exploit how a human *uses* the password.

## Deviations from the master prompt

| Spec | Implemented | Reason |
|---|---|---|
| `uniformIndex` virtual in `IRandomProvider` | non-virtual, built on pure-virtual `nextUInt64()` | one audited rejection-sampling implementation |
| `generate()` only | `generateInto(…, sink)` + non-virtual `generate` | streaming without complicating the single-result call |
| `GenerationContext&` | `const GenerationContext&` | non-owning service bundle |
| cancellation "C++20 mechanism" | project-owned `CancellationToken/Source` | libc++ lacks `std::stop_token` without `-fexperimental-library` (verified) |
| `configurationSchema` as `QJsonObject` | typed `ConfigurationSchema` + `toJson()` | type-safe generic validation |
| `registerGenerator` → `bool` | `Result<void, RegistryError>` | callers learn why |
| `find` → `IGenerator*` | `shared_ptr<const IGenerator>` | safe unregistration during a run |
| `GeneratorRequest{id, params, count}` | + `unique` | batch uniqueness is request-level |
| metadata | + `outputFields`, `requiresSecureRandom` | drives policy and RNG enforcement generically |
| `DeterministicRandomProvider` next to the secure one | separate library `ar_random_testing` | keep it out of production composition |
| `IGeneratorPlugin::registerGenerators` → `void` | → `Result<void, RegistryError>` + `kGeneratorApiVersion` | failure isolation and compatibility check; still **no loader** |
| `vcpkg-configuration.json` | present, baseline pinned to a real SHA | see `docs/DEPENDENCIES.md` (not fully reproducible) |

## Security model

* Randomness only through `IRandomProvider`. `SecureRandomProvider` is **fail-closed**: bytes come
  from the OS CSPRNG (`getrandom` / `getentropy` / `BCryptGenRandom`) or an exception is thrown; Qt's
  random generator is not used because it can fall back silently (`docs/RANDOMNESS_AUDIT.md`).
  Secure generators reject non-CSPRNG providers; the only way to build a context that accepts an
  insecure provider is a helper in `testsupport/`, whose include path only `ar_random_testing`
  exports, so production targets cannot compile it.
* A failing source yields `RandomSourceFailure` and no records.
* Resource ceilings come from `ApplicationLimits` (`maxBatchSize`, `maxStringLength`,
  `maxParameterCount`, `maxOutputLength`, `maxUniquenessAttempts`); limits can only tighten
  generator-intrinsic bounds. Retry loops are bounded.
* Sensitive data: `docs/SENSITIVE_DATA.md` (enforced vs contract vs planned, and what memory-wiping
  does **not** guarantee).

## Known limitations (security and engineering)

* **Memory:** plaintext copies exist in `QString`, `QJsonObject`, `QSet<QString>` (unique batches),
  sinks, UI; `secureWipe` clears only internal scratch buffers; no guarantee against memory disclosure.
* **No consumers yet** for the sensitive-data contract (logger, persistence, export, clipboard).
* **OS entropy backends:** Linux is compiled and tested (including real-syscall fault injection);
  the macOS and Windows backends were written to the documented APIs but **never compiled**.
* **Unicode classification** depends on the Qt version.
* **Qt types in public interfaces** couple a future plugin loader to Qt/compiler ABI.
* **Throughput:** `SecureRandomProvider` makes one `getrandom` call per 64-bit word: ~1.9 M words/s and
  ~31 k 24-character passwords/s on the sandbox core (slower than the earlier Qt-based provider, which
  preferred the CPU RNG and could not report failure). A buffered variant is a candidate optimisation,
  not done; security outranks speed here.
* **Platforms:** only GCC/Clang on Linux were exercised; MSVC, macOS and Qt ≠ 6.4.2 are not verified.
* **vcpkg path never executed.**
* **No CI configuration** exists yet (out of Slice 1.1 scope).

## Next

Slice 2 only after the Slice 1.1 report is reviewed and approved.
