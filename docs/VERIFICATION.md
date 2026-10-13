# Historical verification record — Slice 1.1 round 4

> **Historical checkpoint.** This document records the Slice 1.1 round-4 core-hardening snapshot before Slice 2 generator integration and before the runtime plugin API v3. Its API-version and “plugin out of scope” statements are historical, not the current project state. For the latest GUI/runtime-plugin source and this sandbox's checks, see `docs/SLICE2_GUI_PLUGIN_VERIFICATION.md`.

## Status

**The round-4 source changes have not received a full C++ build/test verification in this review
sandbox.** The source archive supplied for review contains the round-3 tree; this work applies the
round-4 corrections on top of that source. Python/tooling and standalone compiler checks below were
executed successfully, but CMake cannot configure the project because Qt 6 development files are
not installed. Therefore no green CTest baseline exists here, and no round-4 C++ test, sanitizer,
TSan, benchmark, or mutation kill count is claimed.

## Environment used for this review

| Component | Actual environment |
|---|---|
| OS / architecture | Linux x86-64, kernel `6.18.44` (container) |
| C++ compiler | GCC 14.2.0 |
| Second compiler | Clang 17.0.0 |
| CMake | 3.31.6 |
| Python | 3.13.5 |
| Qt development package | **Missing** (`Qt6Config.cmake` / `qt6-config.cmake` not found) |
| Ninja | Not relied on for the standalone checks |

These versions describe the current review environment, not the earlier round-3 environment.
The previous report's GCC 13.3 / Clang 18 / Qt 6.4.2 results apply only to the round-3 source tree.

## A. Checks actually executed in round 4

| Command/check | Actual result | What it establishes |
|---|---|---|
| `python3 tools/check_tsan.py --self-test` | **PASS: 32 self-test assertions** | Checker accepts the known synthetic teardown signature and rejects generic watchdog/worker races, project frames, unparseable races, and bad exit statuses. |
| `python3 tools/expect_compile_failure.py` with GCC 14.2 for copy-constructor attack | **PASS** | Compiler rejects a derived class that copies the `IRandomProvider` base subobject. |
| Same compile-fail probe with GCC 14.2 for copy-assignment attack | **PASS** | Compiler rejects assignment of the `IRandomProvider` base subobject. |
| Same two compile-fail probes with Clang 17.0 | **PASS** | Both compiler-negative cases are rejected by a second compiler. |
| Compile `src/domain/random/IRandomProvider.cpp` with GCC strict warnings and `-Werror` | **PASS** | The standalone, Qt-independent randomness interface implementation compiles cleanly. |
| Compile `src/domain/random/IRandomProvider.cpp` with Clang strict warnings and `-Werror` | **PASS** | Same isolated compile check with Clang. |
| `python3 -m py_compile tools/check_tsan.py tools/expect_compile_failure.py tools/mutation/run_mutations.py` | **PASS** | Python syntax checks. |
| `bash -n tools/verify.sh` | **PASS** | Shell syntax check. |
| Validate all `tools/mutation/mutations.json` old-text patterns against the final source | **PASS: 42/42 match exactly once** | Mutation definitions are syntactically applicable; this is not evidence that the mutations were killed. |

The initial Clang compile-fail probe exposed a diagnostic-wording assumption in
`expect_compile_failure.py` (`call to deleted constructor`); the script was updated to recognise that
compiler wording. All four GCC/Clang negative probes were rerun and passed after the change.

## B. Full project verification blocked

Command attempted:

```bash
cmake --preset dev-debug
```

Actual result: **configure failed before generating build files**, at `find_package(Qt6 6.4 REQUIRED ...)`:

```text
Could not find a package configuration file provided by "Qt6"
(Qt6Config.cmake / qt6-config.cmake).
```

`pkg-config --modversion Qt6Core` also reports that Qt6Core is unavailable. An attempt to refresh
APT package metadata (`apt-get update -qq`) timed out. Because configure did not succeed, the following
round-4 checks could not be run here:

- CTest: unit, integration, security, robustness, compile-fail CTest entries as part of the full suite;
- Debug build and Release `-Werror` build;
- full Clang project build;
- ASan/UBSan;
- TSan `concurrency_check` and Qt Test suites;
- benchmark;
- `tools/mutation/run_mutations.py` against a green CTest baseline.

**No round-4 C++ suite pass count and no round-4 mutation kill count is claimed.** The `36/36` figure
in the supplied round-3 report belongs to a previous source tree and is not evidence for this tree.
`docs/mutation-report.md` deliberately marks M37-M42 and the full round-4 mutation run as not run.

## C. Round-4 behavior added but requiring full-suite execution

- `IRandomProvider` copy/move construction and assignment are deleted; runtime regression checks and
  compiler-negative fixtures cover the copy constructor and copy-assignment attack.
- TSan's `KNOWN-QTTEST` class requires a parsed data race, the watchdog as an actual racing thread,
  no project frames, and the known `QWaitCondition::~QWaitCondition` / `QTest::qRun` symbols paired
  with their expected Qt Core / Qt Test module names on the same stack frames. A generic race, or
  a report where symbols and module names occur in unrelated frames, is expected to be `UNEXPECTED`.
- `IGenerator::generateInto()` is virtual-final and wraps `generateIntoImpl()` with a validating sink.
  Records must contain every declared output field, must not contain undeclared keys, and must not
  contain object/array values.
- Batch `generate()` returns a `Cancelled` error and no records when cancellation happens after
  partial work. Streaming remains non-transactional; valid records already passed to an external sink
  cannot be rolled back if a later record is invalid or generation is cancelled.
- `kGeneratorApiVersion` is 2 because the generator implementation hook is a breaking extension-contract
  change.

## D. Clean-archive verification (executed against the generated ZIP)

The final ZIP was extracted to a new directory separate from the working tree. These checks were
executed from that fresh extraction:

| Check | Actual result |
|---|---|
| `unzip -t ar_slice1_1_r4.zip` | PASS; no archive integrity errors |
| Extracted file set vs. SHA-256 inventory | PASS; 93/93 files, byte sizes and hashes match |
| Python syntax, Shell syntax, JSON manifests | PASS |
| `python3 tools/check_tsan.py --self-test` | PASS; all 32 checker self-test assertions |
| Standalone `IRandomProvider.cpp` compile, GCC 14.2 and Clang 17 | PASS with strict warnings and `-Werror` |
| Copy-constructor/assignment negative probes, GCC and Clang | PASS; all four invocations reject the forbidden operations |
| Mutation pattern applicability audit | PASS; 42/42 old-text patterns match once; **not** a mutation-kill result |
| `cmake --preset dev-debug` from the clean extraction | BLOCKED/FAIL at configure because Qt6 development files are not installed |

The configure failure occurs at `find_package(Qt6 6.4 REQUIRED ...)` because `Qt6Config.cmake` and
`qt6-config.cmake` are absent. As a result, there is no clean-room build or CTest result for this
round-4 archive. It must be configured, built, and tested on a Qt-enabled Linux runner before Slice 1.1
can be approved.

## E. Platforms not verified

MSVC, Windows, Apple Clang, macOS, the macOS `getentropy` backend, the Windows `BCryptGenRandom`
backend, Qt other than 6.4.2, the vcpkg manifest build, 32-bit targets, and CI remain unverified.
The previous round-3 Linux results are historical only and do not replace testing the round-4 source.
