# Account Recommendations — C++20 / Qt 6

**Source version:** 0.2.0 (Slice 2 core + Qt Widgets GUI + opt-in runtime plugin integration).

Extensible synthetic account-data generator framework and desktop app. The shared core owns the generator contracts, registry, configuration schema, cancellation model, sensitive-data policy, and plugin API. Built-in generators include the configurable password generator and six synthetic identity generators: username, display name, nickname, email-like identifier, account ID, and profile metadata.

The Qt Widgets app (`account_recommendations`) builds its generator navigation and configuration form from registry metadata. Generation runs asynchronously; sensitive values are masked by default; copy and export use channel-specific confirmation and the sensitive-data policy. It supports TXT/TSV, CSV, and JSON output, Light/Dark/System palette actions, and an opt-in runtime plugin loader.

Email-like identifiers use reserved/example domains only and perform no network lookups. Name datasets are local. The application does not implement account registration, verification/CAPTCHA bypass, spam, credential stuffing, account takeover, or access to real accounts.

## Prerequisites

- C++20 compiler and CMake >= 3.21; Ninja recommended.
- Qt 6.4 or newer development packages: Core, Gui, Widgets, Concurrent, Test.
- Runtime plugins must be built against a compatible Qt/C++ ABI and the host's shared `ar_core` library.

On Debian/Ubuntu, the main Qt package is commonly provided by `qt6-base-dev`.

## Build, test, run

```sh
cmake --preset dev-debug
cmake --build --preset dev-debug --parallel 2
ctest --preset dev-debug --output-on-failure

# GUI executable (path may differ by generator/platform)
./build/dev-debug/src/gui/account_recommendations

# Developer CLI
./build/dev-debug/tools/cli_demo/ar_cli_demo --count 3 --length 24
```

The example native plugin is built at `build/dev-debug/plugins/` (actual preset output path follows the configured binary directory). The GUI does not scan or load plugins automatically. Use **Plugins → Load native plugin…**, inspect the warning, and confirm only if you trust the selected code.

CMake options:

- `AR_BUILD_GUI=ON`: Qt Widgets desktop application and smoke test.
- `AR_BUILD_RUNTIME_PLUGINS=ON`: opt-in `QPluginLoader` manager.
- `AR_BUILD_EXAMPLE_PLUGIN=ON`: example native module and runtime integration test.
- `AR_BUILD_TESTS=ON`: unit/integration/security/property/robustness/UI/plugin tests.
- `AR_BUILD_BENCHMARKS=OFF`, `AR_BUILD_TOOLS=ON`, `AR_WARNINGS_AS_ERRORS=OFF`.

## Important trust boundaries

Native plugins are not sandboxed. They execute with the app's privileges, can read or modify files and access the network, and can crash/corrupt the host. Loading is disabled by default and the GUI prompts before each load. The loader checks the Qt plugin IID and generator API version, stages generator registration, and commits the generator set atomically. Modules are intentionally kept resident until process exit; hot-unload is not offered because active jobs may hold plugin-defined vtables.

Password and other sensitive values are masked by default. The GUI applies `SensitiveDataPolicy` on display, clipboard, and export. Clipboard clearing after 30 seconds is best-effort, not a guarantee of erasure from other processes or OS history. No generated results or passwords are saved automatically.

## Documentation

- `docs/SLICE1.md`, `docs/SLICE2.md` — core and generator contracts.
- `docs/GUI.md` — dynamic forms, async generation, sensitive output, and export.
- `docs/PLUGIN_DEVELOPMENT.md` — plugin API v3, build requirements, trust model, and example plugin.
- `docs/SLICE2_GUI_PLUGIN_VERIFICATION.md` — verification actually available in the current environment.
- `docs/VERIFICATION.md`, `docs/SLICE2_VERIFICATION.md`, `docs/SENSITIVE_DATA.md`, `docs/RANDOMNESS_AUDIT.md`, `docs/API_AUDIT.md`, `docs/PORTABILITY.md`, `docs/DEPENDENCIES.md`.

## Current verification caveat

The current sandbox does not contain Qt 6 development files. `cmake --preset dev-debug` has been attempted and stops at `find_package(Qt6 ...)`. Consequently the GUI and runtime plugin C++ targets, full CTest suite, sanitizers, and mutation kill counts have **not** been claimed as passing here. Run the commands above on a Qt-enabled runner before treating this source snapshot as compile/runtime verified.
