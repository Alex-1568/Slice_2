# Runtime plugin development

## Security and ABI contract

Runtime loading is **disabled by default**. The user must select a native shared library and accept a warning before the GUI approves its containing directory for the current session. A plugin runs with the full privileges of the application process. It can read/write files, access the network, execute subprocesses, and crash or corrupt the host. `QPluginLoader` is not a sandbox; do not load code from an untrusted source.

Plugin ABI compatibility requires generator API version **3**, a compatible Qt 6 build, the same compatible C++ compiler/standard-library/runtime ABI, and linkage to the host's shared `ar_core` library. Qt's own plugin metadata provides additional Qt-build checks, but the application does not promise compatibility across arbitrary compiler or runtime combinations. Rebuild plugins against the SDK matching the host release. API version checking is performed after Qt instantiates the plugin object, so it is not a security boundary against malicious native code.

The host reads the embedded plugin IID before instantiating the plugin, then checks `pluginId`, `pluginVersion`, and `apiVersion`. Registration first occurs in an isolated staging registry; the whole staged generator set is validated and then committed atomically to the host registry. Duplicate IDs or any registration failure do not publish a partial set.

The plugin manager itself is not thread-safe; perform enable/load/list operations on one serialized control thread (the GUI thread in the desktop app). Directory scans load candidates sequentially and are not transactional across an entire directory: if a later plugin fails, earlier successful plugins may remain registered.

Loaded modules use `QLibrary::PreventUnloadHint`. There is intentionally no hot-unload function: the registry and background generation jobs can hold shared pointers to plugin-defined generator vtables. Plugins remain loaded until process exit; restart the application to remove one. Native code exceptions are caught at host call boundaries where practical, but crashes, deadlocks, memory corruption, and undefined behavior cannot be contained in-process.

## Implementing a plugin

1. Implement a QObject that also implements `domain::IGeneratorPlugin`.
2. Add `Q_OBJECT`, `Q_PLUGIN_METADATA(IID AR_GENERATOR_PLUGIN_IID)`, and `Q_INTERFACES(account_recommendations::domain::IGeneratorPlugin)`.
3. Return a stable kebab-case `pluginId`, semantic `pluginVersion`, and `domain::kGeneratorApiVersion`.
4. Register one or more `IGenerator` implementations. Each generator must declare its schema and every output field, use `generateIntoImpl`, honor cancellation, and avoid logging generated values.
5. Build a Qt MODULE target linked to the host's shared `ar_core`. Do not statically embed a second copy of `ar_core` into the plugin.
6. Test the plugin against the actual host release/toolchain. A successful compile against headers alone is not ABI verification.

The project builds `plugins/example/ExamplePlugin.cpp` as `ar_example_generator_plugin` to demonstrate the interface. It registers two harmless synthetic generators. The runtime integration test verifies default-off behavior, successful loading, result generation, and atomic failure when one ID collides. This is a demonstration plugin, not a sandbox or a trust-validation system.

## CMake options

- `AR_BUILD_GUI=ON`: build the Qt Widgets desktop application.
- `AR_BUILD_RUNTIME_PLUGINS=ON`: build the opt-in loader (`QPluginLoader`).
- `AR_BUILD_EXAMPLE_PLUGIN=ON`: build the example module under `build/plugins/`.

For development, enable build warnings and run the runtime plugin integration test plus the GUI smoke test. Native plugins should only be loaded from explicitly selected and trusted locations.
