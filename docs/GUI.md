# Qt Widgets GUI

## What is implemented

The `account_recommendations` executable is a Qt 6 Widgets desktop frontend over the same registry and generator contracts as the CLI. The sidebar is generated from `GeneratorRegistry::list()` and groups generators by metadata category. Configuration editors are generated from the application's `ConfigurationSchema` (`Integer`, `Boolean`, `String`, `Enumeration`); generator-specific `switch` statements are not used by the GUI.

Generation runs through `QtConcurrent` and a `QFutureWatcher`, not on the UI thread. A cooperative `CancellationSource` is used per task. The window keeps a job guard until the GUI thread consumes the completion event and waits for any active worker during shutdown so the referenced secure RNG provider and plugin-owned generator remain alive until the task finishes. The table is a `QTableView`/`QStandardItemModel`; interactive batches are bounded to 10,000 rows even though the core may accept a larger batch for non-GUI consumers.

The built-in GUI includes:

- generator/category navigation and schema-driven configuration;
- batch count and optional uniqueness control when supported by metadata;
- generated-result table with output columns from `outputFields`;
- sensitive fields masked by default, with an explicit reveal toggle;
- copy-selected-row with channel-specific confirmation and best-effort clipboard clearing after 30 seconds;
- TXT/TSV, CSV, and JSON bulk export via `QSaveFile`, with confirmation before releasing sensitive data;
- Light, Dark, and System palette actions;
- a native plugin load action that displays a full-privilege warning.

The GUI does not save generated results, passwords, or workspace state. Displaying or copying values creates normal Qt/C++ memory copies; clipboard clearing is best-effort and cannot erase copies already made by other processes.

## Build and smoke test

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

`tests/unit/tst_gui_smoke.cpp` exercises a metadata-selected generator through the UI under Qt's offscreen platform plugin. The complete test has to run on a machine with Qt 6 Core, Gui, Widgets, Concurrent, and Test development components installed.
