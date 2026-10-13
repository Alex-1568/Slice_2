# GUI and runtime plugin integration change list

## Added

- `src/gui/MainWindow.hpp/.cpp`: Qt Widgets main window with registry-driven categories, schema-driven configuration form, async generation, cooperative cancellation, results table, sensitive reveal toggle, explicit clipboard/export confirmation, atomic file export (TXT/TSV, CSV, JSON), and theme palette actions.
- `src/infrastructure/plugins/RuntimePluginManager.hpp/.cpp`: explicit opt-in Qt `QPluginLoader` manager. It requires a caller-approved canonical directory, checks Qt plugin IID before instantiation, validates plugin identity/version/API, stages generator registration, atomically commits the staged generator set, and retains loaded modules for process lifetime.
- `plugins/example/ExamplePlugin.hpp/.cpp`: native plugin example exposing `example-tag` and `example-counter`.
- `tests/unit/tst_runtime_plugin.cpp`: default-off, trust-directory, wrong-IID, atomic-conflict, loading, and generation integration tests.
- `tests/unit/tst_gui_smoke.cpp`: headless GUI smoke test selecting a metadata-backed generator and generating records.
- `docs/GUI.md`, `docs/PLUGIN_DEVELOPMENT.md`, `docs/SLICE2_GUI_PLUGIN_VERIFICATION.md`.

## Changed

- `ar_core` is now a shared library so the application and plugin use one registry/type implementation rather than separate static copies.
- `IGeneratorPlugin` API/IID is version 3. Plugins must be rebuilt against the corresponding Qt/C++ ABI.
- `GeneratorRegistry::registerGeneratorsAtomically()` publishes all plugin generators as one transaction after validating their captured metadata snapshots and checking conflicts.
- CMake adds GUI/concurrent, runtime-plugin, example-plugin, install, and integration-test targets.
- README and audit/dependency documentation now describe the GUI/plugin layers and current verification limitation.

## Follow-up source-review corrections

- The GUI keeps the watcher/job guard alive until its queued completion handler runs, and tears down an active worker/watcher in a defined order.
- Export suffixes are derived from the selected JSON/CSV/text filter when the user omits one.
- The runtime manager's non-thread-safe contract and non-transactional directory-scan behavior are explicit in its public documentation.
- The CMake project and vcpkg package versions are aligned at `0.2.0`.

## Verification limitation

Qt 6 development packages are not installed in the current environment. A normal `cmake --preset dev-debug` run stops at `find_package(Qt6 ...)`; the C++ GUI/plugin tests and full sanitizer/mutation runs have not been claimed as passing.
