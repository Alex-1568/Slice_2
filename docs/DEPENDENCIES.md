# Dependency management

## Status: pinned, but NOT fully reproducible yet

| Item | State |
|---|---|
| vcpkg registry baseline | **Pinned** in `vcpkg-configuration.json` to commit `2cfff9c458d9dcf642e9fa09ba624f9931bb5358` (the real `master` HEAD of `microsoft/vcpkg`, obtained with `git ls-remote https://github.com/microsoft/vcpkg.git HEAD` on 2026-10-07). The SHA was read from the remote, not typed from memory. |
| Version `qtbase` resolves to at that baseline | **6.11.2** (read from `versions/baseline.json` at that commit; `ports/qtbase/vcpkg.json` there also lists a `testlib` feature in its default set) |
| Qt version actually used to compile and test this project | **6.4.2** (Ubuntu 24.04 `qt6-base-dev`) |
| A vcpkg manifest build (`cmake --preset vcpkg-release`) | **Never executed.** The sandbox cannot build Qt from source in reasonable time. |
| vcpkg tool version | not recorded (the tool was never run) |

Consequences, stated plainly:

* The project has **not** been compiled against Qt 6.11.2. Treat the vcpkg path as
  "pinned and plausible", not "verified". The distance 6.4.2 -> 6.11.2 is exactly the kind
  of gap where deprecations or behaviour changes (for example Unicode-table updates that
  affect `QChar::category`) can bite.
* `vcpkg.json` depends on plain `qtbase` (default features: heavy, includes GUI/SQL/network).
  A slimmer feature set (`default-features: false` plus `testlib`, `thread`) is likely
  possible but was not tried, so it is not claimed.
* Because of the above, dependency setup is **not** described anywhere in this repository as
  "fully reproducible".

## Toolchains used in the prior round-3 baseline (not the current round-4 tree)

See `docs/VERIFICATION.md` for the current review environment. These previous versions are historical
context only and their green build claims do not cover round-4 changes.

| Component | Version |
|---|---|
| OS | Ubuntu 24.04 (x86-64, 1 CPU core) |
| GCC / libstdc++ | 13.3.0 |
| Clang (+libstdc++, +libc++ compile-only) | 18.1.3 |
| CMake / Ninja | 3.28.3 / system package |
| Qt | 6.4.2 (system package, not vcpkg) |

## How to lock your own baseline later

```sh
git clone https://github.com/microsoft/vcpkg.git && ./vcpkg/bootstrap-vcpkg.sh
cd account-recommendations
VCPKG_ROOT=../vcpkg cmake --preset vcpkg-release       # first real manifest build
../vcpkg/vcpkg x-update-baseline --add-initial-baseline # re-pins to the vcpkg you tested
git add vcpkg.json vcpkg-configuration.json
```

After the first successful manifest build, record the vcpkg tool version, the resolved Qt
version and the compiler in `docs/VERIFICATION.md`, and only then claim reproducibility.

## Qt minimum

`CMakeLists.txt` requests `find_package(Qt6 6.4 ...)`: 6.4 is the oldest version that has
been compiled and tested, not a statement that nothing older works.


## GUI/runtime plugin integration requirements

The current CMake defaults also request Qt 6 Gui, Widgets, Concurrent, and Test (when tests are enabled). The Qt SDK currently available in the source's historical verification report was 6.4.2, but the GUI and runtime plugin integration were added after that checkpoint and have **not been compiled in the current sandbox**. The `ar_core` target is now a shared library by design so host and native plugins use one registry/type implementation rather than embedding independent static copies. Native plugins must use a compatible compiler, C++ runtime, Qt SDK, and `ar_core` ABI. Qt runtime deployment (for example `windeployqt`/`macdeployqt` or platform package/AppImage setup) has not been verified here.
