# Historical verification record — Slice 2 core generators (pre-GUI/plugin)

> This document records the earlier Slice 2 identity/template-generator checkpoint. The current source also includes the Qt Widgets GUI and runtime plugin layer; its verification status is in `docs/SLICE2_GUI_PLUGIN_VERIFICATION.md`.

## Status

Slice 2 source, CMake targets, tests, CLI registry integration, mutation definitions, and documentation
have been added. **A full C++ configure/build/test run has not passed in the current environment because
Qt 6 development files are unavailable.** This is an environment blocker, not proof that the source
fails to compile. No CTest pass count, sanitizer result, benchmark result, or mutation kill count is
claimed for the Slice 2 tree.

## Environment

| Component | Observed value |
|---|---|
| OS / architecture | Linux x86-64 container |
| CMake | 3.31.6 |
| GCC | 14.2.0 |
| Clang | 17.0.0 |
| Python | 3.13.5 |
| Qt 6 development package | Missing; no `Qt6Config.cmake` found under `/usr` or `/opt` |

These versions describe this review sandbox and do not imply that Slice 2 was compiled with them.

## Checks executed

| Check | Result | Interpretation |
|---|---|---|
| `cmake --preset dev-debug` | **BLOCKED** at `find_package(Qt6 6.4 REQUIRED ...)` | No Qt target could be generated. |
| `python3 -m py_compile tools/check_tsan.py tools/expect_compile_failure.py tools/mutation/run_mutations.py` | **PASS** | The Python tooling parses; this does not verify C++ behavior. |
| `bash -n tools/verify.sh` | **PASS** | The verification shell script has valid shell syntax. |
| `python3 tools/check_tsan.py --self-test` | **PASS** | Synthetic TSan checker cases pass; no actual TSan C++ executable was built or run. |
| Four `expect_compile_failure.py` probes (GCC 14.2, Clang 17; copy construction/assignment attacks) | **PASS** | Compilers reject the prohibited `IRandomProvider` operations. These fixtures are Qt-independent and primarily protect the inherited Slice 1.1 boundary. |
| Compile `src/domain/random/IRandomProvider.cpp` with GCC and Clang plus `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` | **PASS** | This single Qt-independent implementation unit compiles cleanly; it is not a whole-project build. |
| Mutation JSON/source-pattern audit | **PASS: 60/60 source patterns occur exactly once in the later GUI/plugin tree** | Mutations have applicable textual targets; no mutation was applied and no kill count is established. |
| Full CTest suite | **NOT RUN** | Requires successful configure/build. |
| GCC/Clang full builds with `-Werror` | **NOT RUN** | Requires Qt Core/Test headers and package config. |
| ASan/UBSan and TSan runtime tests | **NOT RUN** | Requires built test executables. |
| Mutation runner | **NOT RUN** | Its required green baseline cannot be produced without Qt. |
| Benchmark | **NOT RUN** | Requires built benchmark executable. |

## Required verification on a Qt-enabled runner

From a clean checkout/archive with Qt 6.4+ Core and Test development files:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug --output-on-failure
cmake --preset ci-release
cmake --build --preset ci-release
ctest --preset ci-release --output-on-failure
python3 tools/mutation/run_mutations.py --only M43 M44 M45 M46 M47 M48 M49 M50 M51 M52 M53 M54
python3 tools/mutation/run_mutations.py
```

Run the existing `sanitizers`, `tsan`, `clang`, `libcxx`, and `benchmark` verification steps where their
prerequisites are available. Preserve raw command output or machine-readable test reports. A mutation
counts as killed only when the runner builds the mutant and the expected test(s) actually fail; an
applicable text pattern is not a mutation result.

## Known verification limitations

- No full Qt build or Qt Test suite for Slice 2 was run in this sandbox.
- No MSVC, Apple Clang, Windows, macOS, 32-bit, or alternate Qt-version claim is made.
- The new C++ files have not received compiler-backed diagnostics here; source review and syntax checks
  cannot replace a real build.
- The mutation catalogue is present as a test plan only until run against a green baseline.

## Clean-room distribution verification

After packaging, `ar_slice2.zip` was extracted into a separate empty directory and the following checks
were repeated against that extracted copy:

- ZIP integrity: **PASS** (`unzip -t`, no errors).
- Source inventory: **PASS**, 121/121 files and SHA-256 values matched the generated inventory with no missing, extra, or changed files.
- Patch reconstruction: **PASS**, applying `slice1.1_to_slice2.patch` to the Slice 1.1 Round 4 baseline reconstructed the exact 121-file project tree in the source archive.
- `python3 -m py_compile tools/check_tsan.py tools/expect_compile_failure.py tools/mutation/run_mutations.py`: **PASS**.
- `bash -n tools/verify.sh`: **PASS**.
- `python3 tools/check_tsan.py --self-test`: **PASS** (all included synthetic scenarios).
- Copy-construction / copy-assignment compile-fail probes: **PASS**, with both GCC 14.2 and Clang 17.
- Standalone compile of `src/domain/random/IRandomProvider.cpp`: **PASS**, GCC and Clang with strict warnings and `-Werror`.
- Mutation catalogue applicability: **PASS**, 60/60 textual targets occur exactly once in the later GUI/plugin tree. This is not a mutation kill result.
- JSON parsing and the identity-source network API/string scan: **PASS**.
- `cmake --preset dev-debug`: **BLOCKED**, exit status 1 at `find_package(Qt6 6.4 ...)` because this sandbox has no Qt6 development package. No C++ target, Slice 2 test, sanitizer, benchmark, or mutation run could be built here.

These clean-room checks verify archive integrity and the Qt-independent checks listed above. They do not substitute
for a Qt-enabled full build/test run. Slice 2 should be considered source-complete but **not yet build-verified**.
