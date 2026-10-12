# Verification record — Slice 1.1 (round 3)

Environment: Ubuntu 24.04 x86-64 (**1 CPU core**), GCC 13.3.0 + libstdc++, Clang 18.1.3, CMake 3.28.3,
Ninja, **Qt 6.4.2 (system package)**, Linux 6.x (Firecracker VM). The sandbox restarted several times
during the session, killing background jobs; every result below was produced by a run that completed on the
final source tree (the mutation run needed a restart-safe runner, see its section).

## A. Reproduced in this round, on the final tree

All via `tools/verify.sh <step>` (each step exits nonzero on any failed condition) unless noted.

| Step | Result |
|---|---|
| `debug` (cmake --preset dev-debug / build / ctest) | 15/15 ctest entries pass = 13 Qt Test suites (**275** test functions/data rows by Qt Test's count) + `concurrency_check` + `tsan_checker_selftest` |
| `release` (`-Werror`) | 15/15, 0 warnings |
| `clang` (Clang 18 + libstdc++, `-Werror`) | 15/15, 0 warnings |
| `sanitizers` (ASan + UBSan) | 15/15, no sanitizer reports |
| `tsan`: `concurrency_check`, strict | exit 0, **0 reports** |
| `tsan`: `tst_registry`, `tst_password_generator`, `tst_robustness`, qttest mode | tests pass; 3 reports each, all classified KNOWN-QTTEST; 0 PROJECT, 0 UNEXPECTED |
| `selftest` | classifier self-test passes (incl. the new cases), 6 failure scenarios make `verify.sh` exit 1, 2 positive controls exit 0 |
| `libcxx` (Clang 18 + libc++) | the four libraries compile; **tests not built or run** |
| `benchmark` | ran to completion (numbers below) |
| Mutation: `python3 tools/mutation/run_mutations.py --workdir DIR --resume` | **36 of 36 killed as expected** (`docs/mutation-report.md`) |
| Clean configure/build/test from the extracted final archive | see the "Clean-room check" section at the end of this file |

Real fault injection ran, not skipped: `tst_secure_random` executed (a) the real OS backend under a
seccomp-denied `getrandom` (reports `EPERM`) and (b) the full production path (attested provider, ordinary
context, real `PasswordGenerator`) under the same fault (returns `RandomSourceFailure`, no records).

### What the three round-3 fixes are backed by

* **RNG trust boundary** (`tst_secure_random`, 14 functions; `static_assert`s; scan tests): injected-source
  providers are never attested; a provider hiding the flag with its own function is rejected; all test doubles are
  untrusted; production code cannot reference the injection/attestation words. Mutations M25, M31, M32 killed.
* **TSan classification** (`tools/check_tsan.py --self-test`, also ctest `tsan_checker_selftest`): the watchdog must
  be one of the threads that performed the racing accesses in a plain data race. Cases: race between two other
  threads with the watchdog named only in creation annotations -> UNEXPECTED (verification fails); watchdog only
  created a racing thread -> UNEXPECTED; non-data-race kind -> UNEXPECTED; unparseable access headers ->
  UNEXPECTED; watchdog as a real racing party -> accepted. The real logs of this round still classify as known.
  Mutations M35 and M36 (loosened rules) killed by `tsan_checker_selftest`.
* **Nested JSON** (`tst_sensitive`): flat-record contract, whole-value handling, no recursion, `validateRecord`.
  Mutations M33, M34 killed.

### TSan: how the verdict is reached (nothing is suppressed)

`check_tsan.py` receives the log and the exit status. *strict*: exit 0 and zero reports. *qttest*: exit 0 or 66,
`Totals: N passed, 0 failed`, and every report must be KNOWN-QTTEST (PROJECT or UNEXPECTED fail; a TSan runtime
abort always fails). Wording for the accepted class: **TSan did not identify project-code frames in the observed
reports; the remaining reports originated from Qt Test/system-library teardown in the tested environment.**
`tst_secure_random` is excluded from the TSan list because it spawns a helper process and libtsan (GCC 13) aborts
with an internal `CHECK failed ... ForkAfter` when a multi-threaded process forks; the checker reported that as a
failure when it first happened, and the exclusion is documented in `tools/verify.sh`. TSan observes only the
interleavings that occurred, on one core.

### Mutation runner

`tools/mutation/mutations.json` lists each defect (file, replaced text, tests that must fail, why). The runner
copies the tree, applies one defect at a time, rebuilds, runs ctest and reports KILLED / KILLED-OTHER / SURVIVED /
STALE / BUILD-ERROR. After a sandbox restart killed a run, it was made restart-safe (per-mutation results file,
`--resume`, files touched so objects built from an interrupted mutant are rebuilt) and the full 36-mutation run was
repeated from scratch; the report comes from that complete run. Mutations show that these 36 defects are noticed,
not that no other defect exists.

### Benchmark (`docs/benchmark-sample.txt`)

One Xeon core @ 2.1 GHz, Release: fail-closed OS provider ~1.8 M `uniformIndex` calls/s and roughly 28-34 k
24-character passwords/s; the deterministic test provider is far faster (see the file). The earlier Qt-based
provider was ~2.3x faster but could not report failure. Measurements on one machine, not targets.

## B. Reports from earlier runs only (NOT reproduced on this tree)

* The 30-mutation results and the clean-room build of the round-2 archive describe the round-2 tree. They are
  superseded by section A and are not evidence for this tree.
* `docs/RANDOMNESS_AUDIT.md` findings about Qt 6.4.2 come from reading upstream source (tag `v6.4.2`); the Ubuntu
  package's patches were not examined, and the empirical Qt observation (exit 20, no error indication) was made
  on this machine only.

## C. Not verified

MSVC, Apple Clang, Windows, macOS (the `getentropy` and `BCryptGenRandom` backends were **never compiled**),
32-bit builds, Qt versions other than 6.4.2 (including 6.11.2 from the pinned vcpkg baseline), the vcpkg manifest
build, any CI configuration, Qt Test suites under libc++, and `tst_secure_random` under TSan. See
`docs/PORTABILITY.md`, `docs/DEPENDENCIES.md`, `docs/RANDOMNESS_AUDIT.md`.

## Clean-room check

An archive cannot contain the result of testing itself, so the check was done in two passes:

1. **Archive A** (the tree immediately before this section was written) was unzipped into an empty directory and
   built with `cmake --preset dev-debug`, `cmake --build --preset dev-debug`, `ctest --preset dev-debug`.
   Result: `100% tests passed, 0 tests failed out of 15`, 0 compiler warnings, exit status 0.
2. **The delivered archive** differs from A only by this section of `docs/VERIFICATION.md` (documentation).
   The same three commands were run again on a fresh extraction of the delivered archive; that result is reported
   in the delivery message, not in this file.
