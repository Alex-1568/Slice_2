# Mutation report — Slice 2 + GUI/runtime plugin catalogue

## Status: NOT RUN in this sandbox

The current catalogue contains 69 mutation definitions. M01-M42 cover core and Slice 1.1 contracts, M43-M55 cover Slice 2 identity/template behavior, M56-M60 cover runtime plugin and GUI controls, and M61-M69 cover generation/input/output boundary, spreadsheet formula handling, plugin consent scope, and job/plugin interaction.

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

A mutation counts as killed only when the mutant builds and the actual set of failing test cases exactly matches the expected set. Build errors, test infrastructure errors, stale patterns, extra failures, or unrun cases do not count as killed.


## Whole-project audit mutations (M61-M69)

| ID | Defect injected | Expected test | Status |
|---|---|---|---|
| M61 | Disable the record-count cap at the public generator boundary | `tst_generator_boundary` | NOT RUN — no Qt build baseline |
| M62 | Ignore downstream `SinkResponse::Stop` | `tst_generator_boundary` | NOT RUN — no Qt build baseline |
| M63 | Remove spreadsheet formula-marker protection | `tst_gui_smoke` | NOT RUN — no Qt build baseline |
| M64 | Accept `Completed` with fewer records than requested | `tst_generator_boundary` | NOT RUN — no Qt build baseline |
| M65 | Leave plugin menu enabled during generation | `tst_gui_smoke` | NOT RUN — no Qt build baseline |
| M66 | Ignore exact-file approval and require broader directory trust instead | `tst_runtime_plugin` | NOT RUN — Qt development package is unavailable |
| M67 | Skip request validation at the public generator boundary | `tst_generator_boundary` | NOT RUN — Qt development package is unavailable |
| M68 | Disable the generic generated-string length limit at the validating sink | `tst_generator_boundary` | NOT RUN — Qt development package is unavailable |
| M69 | Allow nested JSON values in request parameters at the public boundary | `tst_generator_boundary` | NOT RUN — Qt development package is unavailable |

Mutation runner note: a mutation is considered `KILLED` only if the actual parsed failing-test set matches the expected set exactly. Missing or malformed JUnit output now yields `TEST-ERROR`; it is never counted as a surviving or killed mutation. This runner improvement itself has only undergone syntax/source inspection in this environment, not a Qt-backed mutation run.
