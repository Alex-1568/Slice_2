# Slice 1 -> Slice 1.1: what changed and why

Generated from a file-level comparison with the delivered Slice 1 archive (see the generated file lists at the end). The full textual diff is `slice1_to_slice1.1.patch` (delivered next to the archive).

## Why each change was made

| Area | Files | Reason |
|---|---|---|
| Entropy semantics | `generators/password/EntropyEstimator.*`, `PasswordGenerator.*`, `tools/cli_demo/main.cpp`, tests | Separate the generator's guaranteed entropy (lower bound) from the search-space size (upper bound); remove the `strength` notion; band from the lower bound only |
| Cancellation portability | `domain/generator/CancellationToken.hpp` (new), `GenerationContext.hpp`, tests | `std::stop_token` is unavailable in libc++ 18 without an experimental flag |
| RNG isolation | `infrastructure/random/testing/*` (moved), `CMakeLists.txt`, `benchmarks/CMakeLists.txt`, `tests/CMakeLists.txt` | Production targets must not be able to link the deterministic provider |
| Centralised limits | `common/limits/ApplicationLimits.hpp`, `ConfigurationSchema.*`, `PasswordConfiguration.*`, `PasswordValidator.*` | Parameter-count, string-length and output-length ceilings from one place; limits only tighten |
| Sensitive-data policy | `domain/security/SensitiveDataPolicy.*`, `docs/SENSITIVE_DATA.md` | Workspace channel; documented contract vs enforcement; no memory-wipe claims |
| Plugin boundary (no loader) | `domain/plugin/IGeneratorPlugin.*` | API-version check and failure isolation for a future loader |
| Reproducible dependencies | `vcpkg-configuration.json`, `docs/DEPENDENCIES.md` | Real baseline SHA; honest limits of reproducibility |
| Verification tooling | `tools/verify.sh`, `tools/mutation/*`, `CMakePresets.json`, `docs/VERIFICATION.md`, `docs/mutation-report.md` | Make every claimed result re-runnable |
| New tests | `tst_entropy_semantics`, `tst_robustness`, `tst_plugin_boundary`, `concurrency_check`; additions to `tst_random`, `tst_security`, `tst_sensitive`, `tst_password_generator`, `tst_password_unit` | Extreme inputs, entropy soundness against an exact distribution, golden vectors from an independent Python implementation, source-scan guards, TSan-friendly concurrency check |
| Docs | `docs/API_AUDIT.md`, `PORTABILITY.md`, `SLICE1.md`, `README.md` | API review, platform honesty, corrections to earlier claims |

## Breaking changes (pre-release, acceptable before Slice 2)

* Password output fields: `strength`, `entropyBits` -> `guaranteedEntropyBits`, `searchSpaceBits`, `entropyQuality`.
* `StrengthLevel`/`StrengthThresholds`/`classifyStrength`/`EntropyEstimate` -> `EntropyBand`/`EntropyBandThresholds`/`classifyBand`/`GeneratorEntropyEstimate`.
* `GenerationContext` takes `CancellationToken` instead of `std::stop_token`.
* `DeterministicRandomProvider` header moved to `infrastructure/random/testing/` and library `ar_random_testing`.
* Password generator metadata version 1.0.0 -> 1.1.0.

## Round 2 (review fixes after the first Slice 1.1 submission)

| Finding in review | Change |
|---|---|
| Qt's system generator is not fail-closed | New `ISystemEntropy` + OS backends (`OperatingSystemEntropy.cpp`), `SecureRandomProvider` rewritten, Qt random classes banned under `src/`, `docs/RANDOMNESS_AUDIT.md`, `tst_secure_random` (fault injection incl. real seccomp-denied `getrandom`), helper `tests/fixtures/seccomp_getrandom_probe.cpp`, CLI demo probes at startup |
| Confirmation gap for undeclared fields | `SensitiveDataPolicy` record-level `requiresConfirmation`, `ReleaseAuthorization`, default redaction for Export/Clipboard; four regression scenarios in `tst_sensitive` |
| `verify.sh` hid TSan failures | `tools/check_tsan.py` (+ `--self-test`), `tsan-concurrency`/`tsan-qttest`/`selftest` steps, `tests/fixtures/tsan_racy.cpp` |
| `forTesting()` in the production header | Removed; `testsupport/testing/{InsecureGenerationContext,DeterministicRandomProvider}` exported only by `ar_random_testing`; scan tests |
| Mutation coverage | 30 mutations (6 new: M25-M30; M23 rewritten for the new provider) |

Breaking (pre-release): `GenerationContext::forTesting` -> `testing::insecureContext` (test support);
`DeterministicRandomProvider` moved to `testsupport/testing/` and namespace `account_recommendations::testing`;
`SensitiveDataPolicy::requiresConfirmation(channel)` removed in favour of the record-level overloads;
`sanitize`/`handling` take an optional `ReleaseAuthorization`.

## Round 3 (second review)

| Finding | Change |
|---|---|
| RNG trust boundary (public injection constructor, self-attested security flag) | `IRandomProvider::isCryptographicallySecure()` non-virtual; `SecureAttestation` token; `SecureRandomProvider` injection constructor private (`testing::EntropyInjectionAccess` in `testsupport/`); `ISystemEntropy::isOperatingSystemSource()` removed; compile-time `static_assert`s and scan tests; production-path seccomp test (`--generate`) |
| TSan classification too loose | `check_tsan.py`: the watchdog must be a racing thread of a plain data race; 5 new negative/positive self-tests; self-test registered as ctest `tsan_checker_selftest` |
| Nested JSON contract | Flat-record contract, `validateRecord`, whole-value handling and fail-closed treatment of objects/arrays in `SensitiveDataPolicy`; `handling()` now takes the value; docs |
| Mutation catalogue | 36 mutations: M25 rewritten, M31-M36 added (attestation, nested JSON, TSan classifier) |

Breaking (pre-release): `isCryptographicallySecure` no longer virtual (test doubles must not override it);
`SecureRandomProvider(std::shared_ptr<ISystemEntropy>)` is private (use `testing::providerWithInjectedSource`);
`FaultInjectingEntropy` lost its "claims OS" parameter; `SensitiveDataPolicy::handling(key, channel, ...)` became
`handling(key, value, channel, ...)`.

## File lists (generated by comparing directory trees; build outputs excluded)

### Round 3 vs the round-2 archive

Added (1):
- `testsupport/testing/InjectedEntropyProvider.hpp`

Removed or moved away (0):

Modified (27):
- `docs/API_AUDIT.md`
- `docs/CHANGES_1_1.md`
- `docs/RANDOMNESS_AUDIT.md`
- `docs/SENSITIVE_DATA.md`
- `docs/VERIFICATION.md`
- `docs/benchmark-sample.txt`
- `docs/mutation-report.md`
- `src/domain/generator/GeneratorMetadata.cpp`
- `src/domain/generator/GeneratorMetadata.hpp`
- `src/domain/random/IRandomProvider.hpp`
- `src/domain/security/SensitiveDataPolicy.cpp`
- `src/domain/security/SensitiveDataPolicy.hpp`
- `src/infrastructure/random/ISystemEntropy.hpp`
- `src/infrastructure/random/OperatingSystemEntropy.cpp`
- `src/infrastructure/random/SecureRandomProvider.cpp`
- `src/infrastructure/random/SecureRandomProvider.hpp`
- `tests/CMakeLists.txt`
- `tests/fixtures/seccomp_getrandom_probe.cpp`
- `tests/support/TestSupport.hpp`
- `tests/unit/tst_password_generator.cpp`
- `tests/unit/tst_secure_random.cpp`
- `tests/unit/tst_security.cpp`
- `tests/unit/tst_sensitive.cpp`
- `testsupport/testing/DeterministicRandomProvider.hpp`
- `tools/check_tsan.py`
- `tools/mutation/mutations.json`
- `tools/mutation/run_mutations.py`

### Cumulative vs the delivered Slice 1

Added (29):
- `docs/API_AUDIT.md`
- `docs/CHANGES_1_1.md`
- `docs/DEPENDENCIES.md`
- `docs/PORTABILITY.md`
- `docs/RANDOMNESS_AUDIT.md`
- `docs/SENSITIVE_DATA.md`
- `docs/VERIFICATION.md`
- `docs/mutation-report.md`
- `src/domain/generator/CancellationToken.hpp`
- `src/domain/plugin/IGeneratorPlugin.cpp`
- `src/domain/plugin/IGeneratorPlugin.hpp`
- `src/infrastructure/random/ISystemEntropy.hpp`
- `src/infrastructure/random/OperatingSystemEntropy.cpp`
- `tests/concurrency/concurrency_check.cpp`
- `tests/fixtures/seccomp_getrandom_probe.cpp`
- `tests/fixtures/tsan_racy.cpp`
- `tests/unit/tst_entropy_semantics.cpp`
- `tests/unit/tst_plugin_boundary.cpp`
- `tests/unit/tst_robustness.cpp`
- `tests/unit/tst_secure_random.cpp`
- `testsupport/testing/DeterministicRandomProvider.cpp`
- `testsupport/testing/DeterministicRandomProvider.hpp`
- `testsupport/testing/InjectedEntropyProvider.hpp`
- `testsupport/testing/InsecureGenerationContext.hpp`
- `tools/check_tsan.py`
- `tools/mutation/mutations.json`
- `tools/mutation/run_mutations.py`
- `tools/verify.sh`
- `vcpkg-configuration.json`

Removed or moved away (2):
- `src/infrastructure/random/DeterministicRandomProvider.cpp`
- `src/infrastructure/random/DeterministicRandomProvider.hpp`

Modified (36):
- `CMakeLists.txt`
- `CMakePresets.json`
- `README.md`
- `benchmarks/CMakeLists.txt`
- `benchmarks/bench_generation.cpp`
- `docs/SLICE1.md`
- `docs/benchmark-sample.txt`
- `src/common/limits/ApplicationLimits.hpp`
- `src/domain/generator/ConfigurationSchema.cpp`
- `src/domain/generator/ConfigurationSchema.hpp`
- `src/domain/generator/GenerationContext.hpp`
- `src/domain/generator/GeneratorMetadata.cpp`
- `src/domain/generator/GeneratorMetadata.hpp`
- `src/domain/random/IRandomProvider.hpp`
- `src/domain/security/SensitiveDataPolicy.cpp`
- `src/domain/security/SensitiveDataPolicy.hpp`
- `src/generators/password/EntropyEstimator.cpp`
- `src/generators/password/EntropyEstimator.hpp`
- `src/generators/password/PasswordConfiguration.cpp`
- `src/generators/password/PasswordConfiguration.hpp`
- `src/generators/password/PasswordGenerator.cpp`
- `src/generators/password/PasswordGenerator.hpp`
- `src/generators/password/PasswordValidator.cpp`
- `src/generators/password/PasswordValidator.hpp`
- `src/infrastructure/random/SecureRandomProvider.cpp`
- `src/infrastructure/random/SecureRandomProvider.hpp`
- `tests/CMakeLists.txt`
- `tests/support/TestSupport.hpp`
- `tests/unit/tst_common.cpp`
- `tests/unit/tst_password_generator.cpp`
- `tests/unit/tst_password_properties.cpp`
- `tests/unit/tst_password_unit.cpp`
- `tests/unit/tst_random.cpp`
- `tests/unit/tst_security.cpp`
- `tests/unit/tst_sensitive.cpp`
- `tools/cli_demo/main.cpp`
