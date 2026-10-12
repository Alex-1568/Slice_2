#!/bin/sh
# Reproducible verification driver. Every claim in docs/VERIFICATION.md was produced
# by one of these steps. Usage:  tools/verify.sh <step> [<step>...]
#
#   debug        configure+build+ctest, Debug
#   release      Release with -Werror (AR_WARNINGS_AS_ERRORS=ON)
#   sanitizers   AddressSanitizer + UBSan, full suite
#   tsan         ThreadSanitizer: concurrency_check must exit 0 with ZERO reports; the Qt Test
#                suites must pass, and every report must be the known Qt Test teardown class
#                (printed, never suppressed). Anything else fails the script.
#   selftest     proves the TSan checks can fail (real data race, masked exit code, failing
#                suite, missing binary) and pass on clean controls
#   tsan-concurrency / tsan-qttest   the two halves of `tsan` without rebuilding; binaries can be
#                overridden with AR_TSAN_CONCURRENCY_BIN / AR_TSAN_QTTEST_BINS (test hooks)
#   clang        Clang + libstdc++ with -Werror, full suite
#   libcxx       Clang + libc++ (the standard library of Apple/LLVM toolchains), COMPILE ONLY
#   benchmark    Release build with benchmarks, runs ar_bench_generation
#
# Requires: cmake>=3.21, ninja, Qt6 (Core, Test). 'clang'/'libcxx' also need clang++ and libc++-dev.
set -eu
cd "$(dirname "$0")/.."
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

step_debug() {
    cmake --preset dev-debug
    cmake --build --preset dev-debug -j "$JOBS"
    ctest --preset dev-debug
}
step_release() {
    cmake --preset ci-release
    cmake --build --preset ci-release -j "$JOBS"
    ctest --preset ci-release
}
step_sanitizers() {
    cmake --preset sanitizers
    cmake --build --preset sanitizers -j "$JOBS"
    ctest --preset sanitizers
}
# ---- ThreadSanitizer -------------------------------------------------------------
# tsan_check NAME BINARY MODE   (MODE: strict | qttest). Runs BINARY, then lets
# tools/check_tsan.py decide. Nothing is swallowed: the exit status of the binary and the
# parsed reports both feed the verdict, and any failure makes this function (and the whole
# script) return nonzero.
tsan_check() {
    name=$1; bin=$2; mode=$3
    mkdir -p build/tsan
    log="build/tsan/$name.log"
    if [ ! -x "$bin" ]; then
        echo "FAIL ($name): $bin is not an executable file"
        return 1
    fi
    set +e
    "$bin" > "$log" 2>&1
    rc=$?
    set -e
    echo "== $name [$mode] exit status $rc"
    python3 tools/check_tsan.py "$log" --mode "$mode" --exit-code "$rc" --project-root "$PWD"
}
step_tsan_concurrency() {
    tsan_check concurrency_check "${AR_TSAN_CONCURRENCY_BIN:-./build/tsan/tests/concurrency_check}" strict
}
step_tsan_qttest() {
    # The default list is the suites with threads. tst_secure_random is deliberately NOT here: it
    # spawns a helper process (QProcess/fork) and libtsan 2 from GCC 13 aborts with an internal
    # "CHECK failed" in ForkAfter when a multi-threaded process forks. That is a TSan runtime
    # limitation, not a project race, and the suite has no concurrency content; it is still run by
    # every other step (debug, release, clang, sanitizers). check_tsan.py reports such an abort as a
    # FAILURE rather than hiding it.
    bins="${AR_TSAN_QTTEST_BINS:-./build/tsan/tests/tst_registry ./build/tsan/tests/tst_password_generator ./build/tsan/tests/tst_robustness}"
    for b in $bins; do
        tsan_check "$(basename "$b")" "$b" qttest
    done
}
step_tsan() {
    cmake --preset tsan
    cmake --build --preset tsan -j "$JOBS"
    step_tsan_concurrency
    step_tsan_qttest
}

# ---- proof that the checks can fail -----------------------------------------------
expect_failure() {  # DESCRIPTION CMD...
    desc=$1; shift
    set +e; "$@" > build/selftest/last.log 2>&1; rc=$?; set -e
    if [ "$rc" -eq 0 ]; then
        echo "SELFTEST FAIL: expected a nonzero exit status for: $desc"
        sed -n '1,12p' build/selftest/last.log
        return 1
    fi
    echo "SELFTEST OK  : $desc -> verify.sh exited $rc"
}
expect_success() {  # DESCRIPTION CMD...
    desc=$1; shift
    set +e; "$@" > build/selftest/last.log 2>&1; rc=$?; set -e
    if [ "$rc" -ne 0 ]; then
        echo "SELFTEST FAIL: expected exit status 0 for: $desc (got $rc)"
        sed -n '1,20p' build/selftest/last.log
        return 1
    fi
    echo "SELFTEST OK  : $desc -> verify.sh exited 0"
}
step_selftest() {
    mkdir -p build/selftest
    echo "== classifier self-test"
    python3 tools/check_tsan.py --self-test
    echo "== end-to-end: verify.sh must fail when a required condition is not met"
    g++ -std=c++20 -g -O0 -fsanitize=thread -pthread tests/fixtures/tsan_racy.cpp -o build/selftest/racy
    printf 'int main(){return 0;}\n' > build/selftest/clean.cpp
    g++ -std=c++20 -g -fsanitize=thread -pthread build/selftest/clean.cpp -o build/selftest/clean
    printf '#!/bin/sh\nexit 3\n' > build/selftest/exit3.sh
    printf '#!/bin/sh\necho "Totals: 3 passed, 1 failed, 0 skipped, 0 blacklisted, 5ms"\nexit 1\n' > build/selftest/qt_fail.sh
    printf '#!/bin/sh\necho "Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted, 5ms"\nexit 0\n' > build/selftest/qt_ok.sh
    chmod +x build/selftest/exit3.sh build/selftest/qt_fail.sh build/selftest/qt_ok.sh
    V="sh tools/verify.sh"
    expect_failure "concurrency binary with a REAL data race" env AR_TSAN_CONCURRENCY_BIN=build/selftest/racy $V tsan-concurrency
    expect_failure "same race with TSan's exit code masked (TSAN_OPTIONS=exitcode=0): report parsing must still catch it" \
        env TSAN_OPTIONS=exitcode=0 AR_TSAN_CONCURRENCY_BIN=build/selftest/racy $V tsan-concurrency
    expect_failure "concurrency binary that exits nonzero without any TSan report" env AR_TSAN_CONCURRENCY_BIN=build/selftest/exit3.sh $V tsan-concurrency
    expect_failure "concurrency binary missing" env AR_TSAN_CONCURRENCY_BIN=build/selftest/does-not-exist $V tsan-concurrency
    expect_failure "Qt Test suite with a failing test" env AR_TSAN_QTTEST_BINS=build/selftest/qt_fail.sh $V tsan-qttest
    expect_failure "Qt Test suite that really races (project frames, no Qt Test watchdog)" env AR_TSAN_QTTEST_BINS=build/selftest/racy $V tsan-qttest
    expect_success "positive control: clean TSan binary in strict mode" env AR_TSAN_CONCURRENCY_BIN=build/selftest/clean $V tsan-concurrency
    expect_success "positive control: passing Qt Test suite" env AR_TSAN_QTTEST_BINS=build/selftest/qt_ok.sh $V tsan-qttest
    echo "selftest: all expectations met"
}

step_clang() {
    cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DAR_WARNINGS_AS_ERRORS=ON
    cmake --build build/clang -j "$JOBS"
    ctest --test-dir build/clang --output-on-failure
}
step_libcxx() {
    cmake -S . -B build/libcxx -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS=-stdlib=libc++ -DAR_BUILD_TESTS=OFF -DAR_BUILD_TOOLS=OFF
    cmake --build build/libcxx -j "$JOBS" --target ar_core ar_random ar_random_testing ar_generators
}
step_benchmark() {
    cmake --preset dev-release
    cmake --build --preset dev-release -j "$JOBS"
    ./build/dev-release/benchmarks/ar_bench_generation
}

[ "$#" -gt 0 ] || { echo "usage: $0 debug|release|sanitizers|tsan|selftest|clang|libcxx|benchmark ..."; exit 2; }
for s in "$@"; do
    echo "######## $s"
    fn="step_$(echo "$s" | tr '-' '_')"
    if ! command -v "$fn" >/dev/null 2>&1; then echo "unknown step: $s"; exit 2; fi
    "$fn"   # set -e: a failing step terminates the script with its nonzero status
done
