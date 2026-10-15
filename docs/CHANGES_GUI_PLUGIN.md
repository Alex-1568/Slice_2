# GUI and runtime plugin integration change list

## Added

- `src/gui/MainWindow.hpp/.cpp`: Qt Widgets main window with registry-driven categories, schema-driven configuration form, async generation, cooperative cancellation, results table, sensitive reveal toggle, explicit clipboard/export confirmation, atomic file export (TXT/TSV, CSV, JSON), and theme palette actions.
- `src/infrastructure/plugins/RuntimePluginManager.hpp/.cpp`: explicit opt-in Qt `QPluginLoader` manager. It requires a caller-approved canonical file or directory (directory approval for scans), checks Qt plugin IID before instantiation, validates plugin identity/version/API, stages generator registration, atomically commits the staged generator set, and retains loaded modules for process lifetime.
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


## Whole-project audit corrections

- Hardened the final `IGenerator::generateInto()` boundary against plugins that overproduce, claim completion with too few rows, continue after `SinkResponse::Stop`, or throw exceptions. Exception messages are not surfaced because they may contain sensitive values. `Stop` consistently counts the current submitted record; streaming side effects before a later error cannot be rolled back, while batch `generate()` does not return partial records.
- Added CSV/TSV formula-like string protection and tests in `ExportFormatting`; numeric JSON values are not rewritten.
- Disabled plugin-menu interaction during generation to prevent selected metadata from changing before completion delivery.
- Made Light and System palettes distinct and clamped schema defaults to `QSpinBox`'s supported integer range.
- Added five mutation definitions (M61-M65) for the new boundary/export/GUI safeguards. They are catalogued but have not been executed because the Qt-enabled baseline is unavailable in this environment.

- Changed GUI plugin consent from implicitly trusting the selected file's parent directory to approving only the exact canonical shared-library file. Directory scans still require explicit directory approval.
- Sanitized plugin-originated messages and exception text at the plugin registration boundary.

- Centralized request validation at `IGenerator::generateInto()` so implementations, including native plugins, cannot bypass it by omitting their own validation call.

- Enforced `ApplicationLimits` parameter count/string and generated-output string-length ceilings at the generic generation boundary, including for plugins that omit local checks.
