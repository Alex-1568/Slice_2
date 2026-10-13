# Mutation report — Slice 2 + GUI/runtime plugin catalogue

## Status: NOT RUN in this sandbox

The current catalogue contains 60 mutation definitions. M01-M42 cover core and Slice 1.1 contracts, M43-M55 cover Slice 2 identity/template behavior, and M56-M60 cover runtime plugin and GUI security controls.

The mutation runner now copies `app/`, `plugins/`, and `resources/` as well as the core source/tests; these directories are required by the default GUI/plugin CMake options. This corrects the runner's previous source-copy list, which would otherwise omit inputs needed to configure the full target graph.

A mutation kill run requires a successful CMake configure/build and a green CTest baseline. In this review environment, a clean configure fails at `find_package(Qt6 6.4 REQUIRED ...)` because Qt 6 development files are not installed. Therefore **no KILLED/SURVIVED count is reported** for this source tree. Source text matching is not mutation execution.

## GUI/runtime-plugin mutations

| ID | Defect injected | Expected test | Status |
|---|---|---|---|
| M56 | Enable native plugin loading by default | `tst_runtime_plugin` | NOT RUN — no Qt build baseline |
| M57 | Skip trusted-directory enforcement | `tst_runtime_plugin` | NOT RUN — no Qt build baseline |
| M58 | Skip Qt plugin IID check before instantiation | `tst_runtime_plugin` | NOT RUN — no Qt build baseline |
| M59 | Ignore failed atomic registry commit | `tst_runtime_plugin` | NOT RUN — no Qt build baseline |
| M60 | Display raw sensitive output without policy sanitization | `tst_gui_smoke` | NOT RUN — no Qt build baseline |

## Reproduce on a Qt-enabled runner

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug --output-on-failure
python3 tools/mutation/run_mutations.py --only M43 M44 M45 M46 M47 M48 M49 M50 M51 M52 M53 M54 M55 M56 M57 M58 M59 M60
python3 tools/mutation/run_mutations.py
```

A mutation counts as killed only when the mutant builds and all expected test cases actually fail. Build errors, stale patterns, or unrun cases do not count as killed.
