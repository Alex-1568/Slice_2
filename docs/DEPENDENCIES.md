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

## Toolchains actually used (see `docs/VERIFICATION.md` for results)

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
