# Account Recommendations — Slice 1 (core + PasswordGenerator)

Extensible synthetic account-data generator framework. **Slice 1** contains the
GUI-free core: generator contract, registry, randomness boundary, sensitive-data
policy, and a complete `PasswordGenerator`, with tests and benchmarks.
There is intentionally **no GUI and no runtime plugin loader** yet.

## Prerequisites

* C++20 compiler. Compiled and tested: GCC 13.3 and Clang 18 on Ubuntu 24.04 (see `docs/VERIFICATION.md`).
  **Not** verified: MSVC, Apple Clang (see `docs/PORTABILITY.md`).
* CMake >= 3.21, Ninja (recommended)
* Qt 6 with **Core** and **Test**. Tested with 6.4.2 only (`find_package(Qt6 6.4 ...)`).

Ubuntu/Debian: `sudo apt install build-essential cmake ninja-build qt6-base-dev`

## Build, test, run

```sh
cmake --preset dev-debug            # or: cmake -S . -B build/dev-debug -G Ninja
cmake --build --preset dev-debug
ctest --preset dev-debug            # unit, integration, property, security, robustness suites

./build/dev-debug/tools/cli_demo/ar_cli_demo --count 3 --length 24 --reveal
```

| Preset | Purpose |
|---|---|
| `dev-debug` | everyday development |
| `dev-release` | optimised + benchmarks (`build/dev-release/benchmarks/ar_bench_generation`) |
| `ci-release` | `-Werror` on project targets |
| `sanitizers` | AddressSanitizer + UBSan |
| `tsan` | ThreadSanitizer, runs the `concurrency` label (see `docs/VERIFICATION.md` for how to read TSan output) |
| `vcpkg-release` | uses `VCPKG_ROOT` toolchain + `vcpkg.json`; baseline pinned, **never executed** (`docs/DEPENDENCIES.md`) |

`tools/verify.sh <step>` runs every verification step (`debug release sanitizers tsan selftest clang libcxx benchmark`); `selftest` proves the TSan checks can fail.
`python3 tools/mutation/run_mutations.py` re-runs the mutation experiment on a scratch copy of the tree.

CMake options: `AR_BUILD_TESTS`, `AR_BUILD_BENCHMARKS`, `AR_BUILD_TOOLS`,
`AR_WARNINGS_AS_ERRORS`, `AR_ENABLE_SANITIZERS`, `AR_ENABLE_TSAN`.

## Layout

```
src/common/        Result<T,E>, ValidationResult, ApplicationLimits, SecureWipe
src/domain/        IGenerator, GenerationContext, metadata/schema, registry,
                   IRandomProvider, SensitiveDataPolicy      (Qt Core only)
src/generators/    password/  (links ar_core only; never sees random implementations)
src/domain/plugin/ IGeneratorPlugin boundary (NO runtime loader)
src/infrastructure/random/   SecureRandomProvider + OS entropy backends (production, fail-closed)
testsupport/testing/        DeterministicRandomProvider, insecure-context helper (include path exported only by ar_random_testing)
tests/ benchmarks/ tools/{cli_demo,mutation,verify.sh} docs/
```

Libraries: `ar_generators -> ar_core`, `ar_random -> ar_core`, `ar_random_testing -> ar_core`.
Generators link neither RNG library, so a generator cannot construct a concrete RNG; production
targets must not link `ar_random_testing` (enforced by a test).

## Adding a generator

1. Implement `domain::IGenerator` (`metadata`, `validate`, `generateInto`).
2. Declare `GeneratorMetadata`: id, schema (`FieldSpec`s), `outputFields` (mark secrets
   `sensitive`), `requiresSecureRandom` if applicable.
3. Call `checkPreconditions(...)` first in `generateInto`; honour `context.isCancelled()`.
4. `registry.registerGenerator(std::make_unique<MyGenerator>())`.
5. Add tests. No framework file needs to change.

## Documentation map

`docs/SLICE1.md` design and corrections - `docs/VERIFICATION.md` commands and evidence -
`docs/SENSITIVE_DATA.md` - `docs/RANDOMNESS_AUDIT.md` - `docs/API_AUDIT.md` - `docs/PORTABILITY.md` - `docs/DEPENDENCIES.md` -
`docs/mutation-report.md`.
