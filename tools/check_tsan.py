#!/usr/bin/env python3
"""Classify ThreadSanitizer output and decide pass/fail. Never hides a report.

Usage:
  check_tsan.py LOG --mode strict|qttest --exit-code N [--project-root DIR]
  check_tsan.py --self-test

strict   the program under test must exit 0 and TSan must have reported NOTHING.
qttest   a Qt Test suite: exit status must be 0 (clean) or 66 (TSan found something, which
         must then be parseable); every test must pass ("Totals: N passed, 0 failed");
         every TSan report must be classified, and only the known environmental class is accepted.

Classes of report
  PROJECT      a stack frame lies in project code (main executable, project path, or the
               account_recommendations namespace)            -> always a failure
  KNOWN-QTTEST no project frame AND the racing threads include Qt Test's 'QtTest Watchdog'
               thread (teardown of Qt Test in an uninstrumented Qt)  -> accepted in qttest mode, printed
  UNEXPECTED   anything else                                  -> failure
A 'FATAL: ThreadSanitizer' line (TSan could not run) is always a failure.
Exit status: 0 = verification passes, 1 = it does not, 2 = usage error.
"""
import argparse, os, re, sys

TSAN_EXIT = 66
FRAME = re.compile(r"^\s+#\d+\s+(.*)$")
MODULE = re.compile(r"\(([^()]*?)\+0x[0-9a-fA-F]+\)\s*$")
TOTALS_OK = re.compile(r"^Totals: \d+ passed, 0 failed", re.M)


def split_reports(text):
    reports, current = [], None
    for line in text.splitlines():
        if line.startswith("WARNING: ThreadSanitizer"):
            if current is not None:
                reports.append(current)
            current = [line]
        elif current is not None:
            current.append(line)
            if line.startswith("SUMMARY: ThreadSanitizer"):
                reports.append(current)
                current = None
    if current is not None:
        reports.append(current)
    return reports


ACCESS = re.compile(
    r"^\s+(?:Previous )?(?:[Aa]tomic )?(?:[Ww]rite|[Rr]ead) of size \d+ at 0x[0-9a-fA-F]+ by (main thread|thread (T\d+))")
THREAD_LINE = re.compile(r"^\s+Thread (T\d+)(?: '([^']*)')? \(")
WATCHDOG = "QtTest Watchdog"


def racing_threads(report):
    """Threads that PERFORMED the conflicting accesses ('main' or 'T<n>'), taken from the
    access headers only. Thread-creation annotations and supporting stacks are not accesses."""
    racers = set()
    for line in report:
        m = ACCESS.match(line)
        if m:
            racers.add("main" if m.group(1) == "main thread" else m.group(2))
    return racers


def thread_names(report):
    return {m.group(1): m.group(2) for m in map(THREAD_LINE.match, report) if m and m.group(2)}


def classify(report, project_root):
    """PROJECT / KNOWN-QTTEST / UNEXPECTED. KNOWN-QTTEST is deliberately narrow: a plain
    data race, no project frame, and Qt Test's watchdog thread is one of the threads that
    actually performed the racing accesses."""
    project_frames = []
    for line in report:
        m = FRAME.match(line)
        if not m:
            continue
        frame = m.group(1)
        mod = MODULE.search(frame)
        base = os.path.basename(mod.group(1)) if mod else ""
        is_system_library = ".so" in base
        if ("account_recommendations" in frame or (project_root and project_root in frame)
                or (mod and not is_system_library)):
            project_frames.append(frame)
    if project_frames:
        return "PROJECT", project_frames
    is_data_race = report[0].startswith("WARNING: ThreadSanitizer: data race")
    names = thread_names(report)
    watchdog_races = any(names.get(t) == WATCHDOG for t in racing_threads(report))
    if is_data_race and watchdog_races:
        return "KNOWN-QTTEST", []
    return "UNEXPECTED", []


def evaluate(text, mode, exit_code, project_root=""):
    """Returns (ok, messages)."""
    msgs = []
    if "FATAL: ThreadSanitizer" in text or "ThreadSanitizer: CHECK failed" in text:
        return False, ["FAIL: the TSan runtime itself failed (FATAL / CHECK failed); the run is not evidence of anything"]
    reports = split_reports(text)
    classes = [classify(r, project_root) for r in reports]
    counts = {"PROJECT": 0, "KNOWN-QTTEST": 0, "UNEXPECTED": 0}
    for cls, _ in classes:
        counts[cls] += 1
    msgs.append(f"TSan reports: {len(reports)} (project={counts['PROJECT']}, "
                f"known-qttest={counts['KNOWN-QTTEST']}, unexpected={counts['UNEXPECTED']})")
    for (cls, frames), report in zip(classes, reports):
        summary = next((l for l in report if l.startswith("SUMMARY")), report[0])
        msgs.append(f"  [{cls}] {summary[:150]}")
        for f in frames[:3]:
            msgs.append(f"      project frame: {f[:140]}")
    ok = True
    if mode == "strict":
        if exit_code != 0:
            ok = False
            msgs.append(f"FAIL: process exit status {exit_code}, required 0")
        if reports:
            ok = False
            msgs.append("FAIL: strict mode requires zero TSan reports")
    else:
        if exit_code not in (0, TSAN_EXIT):
            ok = False
            msgs.append(f"FAIL: exit status {exit_code} (expected 0, or {TSAN_EXIT} when TSan reported)")
        if exit_code == TSAN_EXIT and not reports:
            ok = False
            msgs.append("FAIL: exit status 66 but no parseable TSan report (cannot classify)")
        if not TOTALS_OK.search(text):
            ok = False
            msgs.append("FAIL: Qt Test did not report 'Totals: N passed, 0 failed'")
        if counts["PROJECT"] or counts["UNEXPECTED"]:
            ok = False
            msgs.append("FAIL: report(s) that are not the known Qt Test teardown class")
    msgs.append("RESULT: " + ("PASS" if ok else "FAIL"))
    return ok, msgs


# ----------------------------------------------------------------------------- self-test
_KNOWN = """WARNING: ThreadSanitizer: data race (pid=1)
  Write of size 8 at 0x1 by main thread:
    #0 operator delete(void*, unsigned long) ../../../../src/libsanitizer/tsan/tsan_new_delete.cpp:150 (libtsan.so.2+0x1)
    #1 QWaitCondition::~QWaitCondition() <null> (libQt6Core.so.6+0x27696b)
    #2 QTest::qRun() <null> (libQt6Test.so.6+0x1)
  Previous read of size 8 at 0x1 by thread T1:
    #0 pthread_cond_wait <null> (libtsan.so.2+0x2)
  Thread T1 'QtTest Watchdog' (tid=2, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x3)
SUMMARY: ThreadSanitizer: data race in QWaitCondition::~QWaitCondition()
"""
_PROJECT = _KNOWN.replace("#2 QTest::qRun() <null> (libQt6Test.so.6+0x1)",
                          "#2 account_recommendations::domain::GeneratorRegistry::find x.cpp:10 (tst_registry+0x10)")
_UNEXPECTED = _KNOWN.replace("'QtTest Watchdog'", "'worker'")
_PATH = _KNOWN.replace("#2 QTest::qRun() <null> (libQt6Test.so.6+0x1)", "#2 f /work/proj/src/a.cpp:1 (libfoo.so+0x1)")
# Race between two OTHER threads; the watchdog's name appears only in thread-creation annotations.
_TWO_OTHERS = """WARNING: ThreadSanitizer: data race (pid=1)
  Write of size 4 at 0x1 by thread T2:
    #0 pthread_cond_wait <null> (libtsan.so.2+0x2)
  Previous write of size 4 at 0x1 by thread T3:
    #0 pthread_cond_wait <null> (libtsan.so.2+0x2)
  Thread T1 'QtTest Watchdog' (tid=2, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x3)
  Thread T2 'worker-a' (tid=3, running) created by thread T1 at:
    #0 pthread_create <null> (libtsan.so.2+0x3)
  Thread T3 'worker-b' (tid=4, running) created by main thread at:
    #0 pthread_create <null> (libtsan.so.2+0x3)
SUMMARY: ThreadSanitizer: data race in pthread_cond_wait
"""
# The watchdog only CREATED one of the racing threads.
_CREATOR_ONLY = _TWO_OTHERS.replace("by thread T3:", "by main thread:")
# The watchdog is a racing party (the other party is a non-main thread): still the known class.
_WATCHDOG_VS_WORKER = _TWO_OTHERS.replace("by thread T2:", "by thread T1:")
# Not a data race at all, although the watchdog is named (e.g. a leaked thread).
_OTHER_KIND = _KNOWN.replace("data race (pid=1)", "thread leak (pid=1)")
# A data race whose access headers cannot be parsed: nothing proves who raced.
_NO_ACCESS = "\n".join(l for l in _KNOWN.splitlines() if "of size" not in l) + "\n"
_FATAL = "FATAL: ThreadSanitizer: unexpected memory mapping 0x1\n"
_CHECK = 'ThreadSanitizer: CHECK failed: tsan_rtl.cpp:253 "((!thr->slot)) != (0)" (0x0, 0x0) (tid=1)\n'

_TOTALS = "Totals: 5 passed, 0 failed, 0 skipped, 0 blacklisted, 10ms\n"


def self_test():
    cases = [
        ("clean run, strict", "", "strict", 0, True),
        ("clean Qt Test run", _TOTALS, "qttest", 0, True),
        ("known Qt Test teardown report is accepted (and printed) in qttest mode", _KNOWN + _TOTALS, "qttest", TSAN_EXIT, True),
        ("known report is NOT acceptable in strict mode", _KNOWN, "strict", TSAN_EXIT, False),
        ("strict: nonzero exit without any report", "", "strict", 3, False),
        ("strict: exit 0 but a report was printed (exit code masked)", _KNOWN, "strict", 0, False),
        ("project frame, even with the watchdog present", _PROJECT + _TOTALS, "qttest", TSAN_EXIT, False),
        ("project path inside a library frame", _PATH + _TOTALS, "qttest", TSAN_EXIT, False),
        ("unexpected report without the watchdog thread", _UNEXPECTED + _TOTALS, "qttest", TSAN_EXIT, False),
        ("race between two OTHER threads; watchdog named only in thread-creation annotations", _TWO_OTHERS + _TOTALS, "qttest", TSAN_EXIT, False),
        ("watchdog only CREATED one of the racing threads", _CREATOR_ONLY + _TOTALS, "qttest", TSAN_EXIT, False),
        ("watchdog IS one of the two racing threads (other is a worker)", _WATCHDOG_VS_WORKER + _TOTALS, "qttest", TSAN_EXIT, True),
        ("watchdog named but the report is not a data race", _OTHER_KIND + _TOTALS, "qttest", TSAN_EXIT, False),
        ("data race with unparseable access headers (cannot prove the watchdog raced)", _NO_ACCESS + _TOTALS, "qttest", TSAN_EXIT, False),
        ("failing Qt Test totals", "Totals: 4 passed, 1 failed, 0 skipped\n", "qttest", 1, False),
        ("missing Qt Test totals", "", "qttest", 0, False),
        ("exit 66 but nothing parseable", _TOTALS, "qttest", TSAN_EXIT, False),
        ("TSan could not start", _FATAL + _TOTALS, "qttest", 1, False),
        ("TSan runtime CHECK failure (e.g. fork in a threaded process)", _CHECK, "qttest", TSAN_EXIT, False),
        ("TSan runtime CHECK failure, strict mode", _CHECK, "strict", TSAN_EXIT, False),
        ("unexpected exit status in qttest mode", _TOTALS, "qttest", 5, False),
    ]
    bad = 0
    for name, text, mode, code, expected in cases:
        ok, _ = evaluate(text, mode, code, "/work/proj")
        status = "ok " if ok == expected else "BAD"
        bad += ok != expected
        print(f"  [{status}] {name}: verification {'passes' if ok else 'fails'}"
              f" (expected {'pass' if expected else 'fail'})")
    class_cases = [
        ("known watchdog race", _KNOWN, "KNOWN-QTTEST"),
        ("watchdog vs worker", _WATCHDOG_VS_WORKER, "KNOWN-QTTEST"),
        ("two other threads", _TWO_OTHERS, "UNEXPECTED"),
        ("watchdog only created a racer", _CREATOR_ONLY, "UNEXPECTED"),
        ("other report kind", _OTHER_KIND, "UNEXPECTED"),
        ("no access headers", _NO_ACCESS, "UNEXPECTED"),
        ("project frame", _PROJECT, "PROJECT"),
    ]
    for name, text, expected in class_cases:
        got, _ = classify(split_reports(text)[0], "/work/proj")
        status = "ok " if got == expected else "BAD"
        bad += got != expected
        print(f"  [{status}] class of '{name}': {got} (expected {expected})")
    print("self-test:", "PASS" if bad == 0 else f"FAIL ({bad} case(s))")
    return 0 if bad == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", nargs="?")
    ap.add_argument("--mode", choices=["strict", "qttest"])
    ap.add_argument("--exit-code", type=int)
    ap.add_argument("--project-root", default="")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.log or not a.mode or a.exit_code is None:
        ap.error("LOG, --mode and --exit-code are required")
    try:
        text = open(a.log, encoding="utf-8", errors="replace").read()
    except OSError as e:
        print(f"FAIL: cannot read {a.log}: {e}")
        return 1
    ok, msgs = evaluate(text, a.mode, a.exit_code, a.project_root)
    print("\n".join(msgs))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
