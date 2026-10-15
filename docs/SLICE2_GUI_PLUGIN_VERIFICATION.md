> Historical environment note: this document records the initial Slice 2 + GUI/plugin audit. For the newest whole-project source corrections and current verification results, see [`PROJECT_AUDIT_2026-10.md`](PROJECT_AUDIT_2026-10.md).

# GUI and runtime plugin verification status

## Source-level implementation

- Metadata-driven Qt Widgets window and schema-driven configuration editor added in `src/gui/`.
- Generation dispatched through QtConcurrent with cooperative cancellation and worker lifetime waiting during window teardown.
- Sensitive display/copy/export paths route through `SensitiveDataPolicy`; bulk file writes use `QSaveFile`.
- Runtime plugin loading is disabled by default, checks trusted canonical directories, reads Qt plugin IID metadata before instantiation, checks API/plugin metadata, stages generator registration, and atomically commits the generator set.
- Native plugin modules are kept loaded until process exit because registered instances and workers can retain plugin-defined vtables.
- Example plugin and runtime integration/GUI smoke tests added.

## Checks actually attempted in the current environment

Command:

```sh
cmake --preset dev-debug
```

Result: **configuration blocked**, because this sandbox has no `Qt6Config.cmake` / `qt6-config.cmake` development package for Qt 6.4 or newer. The configuration fails at `find_package(Qt6 ...)` before any GUI/plugin C++ target can compile.

Therefore the following are **not yet verified by execution** for this source tree:

- full Debug or Release C++ build;
- CTest unit/integration/security/property/robustness suite;
- `tst_runtime_plugin` loading the example native module;
- `tst_gui_smoke` under the offscreen Qt platform (including the new sensitive-password masking/reveal regression);
- ASan/UBSan or TSan on the GUI/plugin targets;
- Windows/MSVC and macOS/Apple Clang builds;
- installation/deployment with Qt's platform-specific runtime deployment tools.

The CMake/source changes and documentation are present, but this is not a substitute for Qt compiler/runtime verification. The mutation catalogue has 60 definitions; no GUI/plugin mutation execution or kill count is claimed in this sandbox. Suggested validation on a Qt-enabled runner:

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel 2
ctest --preset dev-debug --output-on-failure
cmake --preset ci-release
cmake --build --preset ci-release --parallel 2
ctest --preset ci-release --output-on-failure
```

For the GUI smoke test, CTest sets `QT_QPA_PLATFORM=offscreen`. For runtime-plugin verification, the example plugin must be built and located at the generated target path. Do not treat this report as an assertion that those commands passed here.


## Additional source review (not a substitute for Qt build)

- The GUI job guard now remains set until the queued `finished()` handler consumes the result, preventing a second request from overwriting the watcher when the worker has stopped but the completion event is still pending. Window teardown cancels/waits and disconnects the watcher before destruction.
- Export extensions now follow the selected file filter when the user omits an extension.
- `RuntimePluginManager` explicitly documents that it is not thread-safe and that directory scans are sequential rather than transactional across multiple files.
- None of these source-level changes has been compiled in this sandbox because the Qt SDK is still unavailable.


## Follow-up review checks after GUI/plugin source cleanup

The final source package also includes a few source-review corrections made after the initial GUI/plugin implementation:

- `MainWindow` now retains its job guard until the queued `QFutureWatcher::finished` handler consumes the result, preventing a second request from overwriting the watcher when the worker has completed but the GUI has not processed the completion event.
- Window teardown requests cancellation, waits if the worker is still active, disconnects the watcher, and deletes it before external services go out of scope.
- Export file extensions now follow the selected JSON/CSV/text filter when the filename has no suffix.
- `RuntimePluginManager` explicitly documents that it is not thread-safe and that directory scans are sequential/non-transactional across multiple plugin files.
- CMake project/package version is aligned at `0.2.0`.
- `tools/mutation/run_mutations.py` now includes `app/`, `plugins/`, and `resources/` in each scratch source copy, which is necessary for the default GUI/plugin build graph. Five unexecuted catalogue entries M56–M60 cover plugin default-off behavior, trusted-directory enforcement, IID checks, atomic registration, and GUI masking.

The following additional checks were executed after these source edits:

| Check | Result | Scope |
|---|---|---|
| `python3 -m compileall -q tools tests` | PASS | Python syntax only; it does not execute C++ or Qt tests. |
| `bash -n tools/verify.sh` | PASS | Shell syntax only. |
| Parse JSON manifests and mutation catalogue | PASS | All project JSON files parsed. The current 60/60 mutation old-text patterns each match exactly once; this is only applicability, not a mutation kill result. |
| `python3 -m compileall -q tools tests` | PASS | Python syntax only. |
| `bash -n tools/verify.sh` | PASS | Shell syntax only. |
| `python3 tools/check_tsan.py --self-test` | PASS | All synthetic checker scenarios passed. No actual TSan C++ executable was run. |
| Four RNG-attestation compile-negative probes (GCC 14.2 / Clang 17) | PASS | Copy construction/assignment of `IRandomProvider` are rejected. |
| Standalone `IRandomProvider.cpp` with GCC and Clang plus strict warnings and `-Werror` | PASS | This one Qt-independent translation unit compiles; not a full-project compile. |
| CMake configure/generate using temporary fake imported Qt targets with AUTOMOC disabled | PASS | CMake target graph and source paths were generated. No source was compiled and Qt's automoc was not run, so this is **not** a Qt build. |
| Normal CMake configure on a clean build directory | BLOCKED (exit 1) | Missing Qt6 development package; fails at `find_package(Qt6 6.4 ...)` before project build generation. |

Generated `__pycache__` and incomplete `build/` directories are excluded from the distribution archive.
