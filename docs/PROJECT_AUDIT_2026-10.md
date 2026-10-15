# Whole-project Audit and Repair Report — October 2026

## Scope and input

The source of truth for this pass was the uploaded delivery bundle `ar_slice2_gui_runtime_plugins_r1_delivery(1).zip` and its nested project source archive `ar_slice2_gui_runtime_plugins_r1.zip`. The original uploaded archive was not overwritten. Repairs were made to a separate working copy and will be delivered in a new ZIP.

This is an existing C++20/CMake/Qt 6 project. It contains the domain/core framework, secure and test-only randomness, password generator, six synthetic identity generators, metadata-driven Qt Widgets GUI, native Qt runtime plugin loader and example plugin, tests, a CLI demo, benchmark source, vcpkg manifest, and technical documentation. External libraries are primarily Qt 6 Core/Gui/Widgets/Concurrent/Test plus standard C++ and platform OS entropy APIs. No database or network service is required by the current application design.

## Defects fixed in this audit

1. **Generation output contract:** the public `IGenerator::generateInto()` boundary now validates output records before releasing them to the destination sink, caps records at `request.count`, prevents a faulty implementation from continuing after `SinkResponse::Stop`, checks the producer's summary against observed submissions, and rejects a `Completed` result with the wrong number of rows. Exceptions from generator/sink code are converted to a generic internal error; exception text is not exposed. The batch convenience API returns no partial record list on error or cancellation. Streaming remains non-transactional by design.
2. **Input validation and resource ceilings:** the same public boundary now invokes `validate(request)` before entering `generateIntoImpl()`, after common preconditions. This prevents a plugin from bypassing its validator merely by forgetting to call it. Top-level parameter count, scalar-only parameter values, parameter key/value string lengths, and generated string length are checked against `ApplicationLimits`. The current configuration-schema model is deliberately flat and scalar; nested object/array parameter values are rejected.
3. **Sink accounting:** built-in identity generators and the example plugin now count the current submitted record before handling the sink response, consistent with the documented rule that a `Stop` response still means the current row was submitted.
4. **Spreadsheet formula injection:** CSV/TSV text fields and headers receive formula-marker protection and appropriate CSV escaping/TSV control-character flattening. JSON export is not rewritten. Numeric JSON values retain numeric representation. Clipboard data intentionally preserves exact text (important for generated passwords), so pasting clipboard values into a spreadsheet remains a separate user-visible risk noted in the GUI documentation.
5. **Native-plugin consent scope:** selecting one plugin in the GUI now approves only that canonical shared-library file for the session. It no longer implicitly approves every library in the same folder. Folder scanning continues to require explicit directory approval.
6. **Plugin error boundary:** exception text and plugin-supplied error messages/validation details are not forwarded through the registration boundary. Stable error categories are retained where available.
7. **GUI state:** the Plugins menu is disabled while generation is running and the file-load slot rechecks the busy state, avoiding a registry/selection metadata change mid-generation. Light and System theme actions now choose distinct palettes. Integer form defaults are clamped to the actual editor range before conversion.
8. **Mutation-runner correctness:** missing or malformed CTest/JUnit output is now an infrastructure `TEST-ERROR`, not a surviving/killed mutation. A mutation is labelled `KILLED` only when the parsed failing-test set exactly matches the expected set; extra or unexpected failures are reported as `KILLED-OTHER`.
9. **Tests and docs:** regression tests were added/updated for the output boundary, input validation, resource limits, formula injection, plugin approval scope, plugin error redaction, and busy-state behavior. The mutation catalogue is extended through M69. The mutation report explicitly marks the full suite not run in this environment.

## Checks actually executed in this environment

Environment: Linux x86-64; GCC 14.2.0; Clang 17.0.0; CMake 3.31.6; Python 3.13.5. Qt 6 development files are not installed.

| Check | Result | Interpretation |
|---|---|---|
| `python3 tools/check_tsan.py --self-test` | PASS, all 32 checker scenarios | TSan log classifier's synthetic positive/negative cases work; this does not run project binaries under TSan. |
| Copy-constructor and copy-assignment attack fixtures, GCC and Clang | PASS, 4/4 invocations rejected as expected | `IRandomProvider` copy/move operations remain structurally disabled. |
| Standalone `IRandomProvider.cpp`, GCC with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` | PASS | The Qt-independent RNG interface implementation compiles cleanly. |
| Same standalone compile with Clang 17 and strict warnings | PASS | Independent compiler check for that translation unit. |
| Python `py_compile` for project tooling | PASS | Syntax only; not execution of a Qt-backed workflow. |
| `bash -n tools/verify.sh` | PASS | Shell syntax only. |
| `python3 -m json.tool tools/mutation/mutations.json` | PASS | Mutation catalogue JSON parses. |
| Mutation target applicability scan | PASS: 69/69 patterns match exactly once | Source pattern readiness only; **not** mutation execution or a kill count. |
| Mutation runner synthetic JUnit controls | PASS: malformed report, missing report, and clean report controls | Verifies the reporting helper does not treat malformed/missing reports as success; does not execute CTest. |
| Common secret-marker scan | No common key/token marker match and no `.env`/private-key/certificate file in the project tree | A limited heuristic scan only; it is not a guarantee that secrets are absent. |
| CMake Debug configure from a clean extraction | BLOCKED | `find_package(Qt6 6.4 REQUIRED ...)` fails because `Qt6Config.cmake` / `qt6-config.cmake` are unavailable. |
| Full CTest / GUI / plugin tests / ASan-UBSan / TSan / benchmark / mutation run | NOT RUN | No configured Qt build exists. |

Package-manager metadata refresh was also attempted earlier in the session but DNS to the Debian package repository was unavailable, so Qt could not be installed in this sandbox. No successful full project build is claimed.

## Dependencies and configuration

No third-party dependency was added by this audit. The existing C++20, CMake, Qt 6, and vcpkg setup remains. A Windows build still links `bcrypt`; the OS entropy implementations use platform-specific backends. Runtime plugins must use a compatible Qt/C++ ABI and the shared `ar_core` library.

The source remains at project version `0.2.0`; the work is a maintenance/audit patch on the existing unreleased project snapshot, not a formal compatibility release. Any change to the third-party generator/plugin ABI must be coordinated with `kGeneratorApiVersion` before publishing compatible binaries.

## Security and residual risks

- Native Qt plugins are code running inside the application process, not sandboxed extensions. Consent, file approval, IID checks and API-version checks cannot constrain a malicious plugin once loaded.
- Directory approval intentionally allows any recognized compatible plugin library under that canonical approved directory; the GUI's single-file flow now uses narrower file approval. Approval is path-based, not a cryptographic content pin; a same-user process able to replace the file can affect what is loaded.
- Apostrophe-prefix CSV/TSV formula protection is a compatibility mitigation, not a universal guarantee for every spreadsheet/import tool. JSON export remains data-faithful. Clipboard preserves exact text; the application warns before releasing sensitive fields and clearing the clipboard is best-effort.
- Plaintext password data can exist in Qt strings, result objects, widgets, the system clipboard, OS history and swap. The project documentation already disclaims guaranteed secure erasure.
- Windows/macOS OS entropy backends, MSVC, Apple Clang, macOS/Windows runtime plugin behavior, Qt versions other than the previously tested 6.4.2, the vcpkg install/build, 32-bit builds, and CI are not verified by this environment.
- The regression test source has been added, but because Qt is absent, this pass cannot establish that the new C++ tests compile or pass. A Qt-enabled runner must configure/build the exact delivered source ZIP, run CTest, sanitizers and the mutation runner before the project can be called fully verified.

## Build and test instructions

On a machine with CMake >= 3.21, a C++20 compiler, Ninja, and Qt 6.4+ development packages (Core, Gui, Widgets, Concurrent, Test):

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel 2
ctest --preset dev-debug --output-on-failure
python3 tools/mutation/run_mutations.py
```

Optional checks documented in `tools/verify.sh` include Release `-Werror`, Clang, ASan/UBSan, TSan, libc++ compile-only and benchmark. Use the exact source ZIP from the delivery bundle; do not substitute an earlier working tree.

## Final source-archive clean-room check

After the source edits were consolidated, a project ZIP was generated from the clean source tree and extracted into a separate directory. The clean-room commands were run from that extracted directory, not from the authoring tree.

| Check on final source ZIP | Result |
|---|---|
| `unzip -t` | PASS; no archive integrity errors |
| Extracted file count vs source tree | PASS: 139 files in each |
| Python tooling syntax, shell syntax and mutation JSON parsing | PASS |
| `python3 tools/check_tsan.py --self-test` | PASS: 32 classifier assertions |
| Standalone `IRandomProvider.cpp`, GCC 14.2 with strict warnings and `-Werror` | PASS |
| Standalone `IRandomProvider.cpp`, Clang 17 with strict warnings and `-Werror` | PASS |
| Copy-constructor and assignment attack probes, GCC + Clang | PASS: all four compiler invocations reject the forbidden operations |
| Mutation pattern applicability | PASS: all 69 patterns match exactly once; this is not a mutation kill result |
| CMake configure from this extraction | BLOCKED: Qt 6.4 development package/configuration files are absent |

The package is internally consistent and the dependency-independent checks above were reproduced from the extracted archive. The blocker at `find_package(Qt6)` occurs before the project's C++ targets are compiled. Consequently, the new/updated Qt unit tests, GUI smoke tests, runtime-plugin tests, sanitizer runs, benchmark and mutation-kill count remain **unverified**, not passed.
