#!/usr/bin/env python3
"""Tiny, dependency-free mutation runner.

For every entry in mutations.json it copies the source tree to a scratch directory,
applies ONE textual defect, rebuilds incrementally, runs ctest and records which tests
failed. It never touches your working tree.

Outcome per mutation
  KILLED        exactly the expected test(s) failed
  KILLED-OTHER  tests failed, but the failing set differed from expected_failures
  SURVIVED      no tests failed -> the test suite has a gap
  STALE         the `old` text no longer occurs exactly once (update mutations.json)
  BUILD-ERROR   the mutant did not compile (inconclusive)
  TEST-ERROR    ctest/JUnit reporting failed, so no test verdict is possible

Exit status is 0 only if every mutation is KILLED.

Usage:
  python3 tools/mutation/run_mutations.py [--only M01 M05 ...] [--report FILE.md]
                                          [--workdir DIR] [--resume] [--keep] [--jobs N]

Restart safety: after each mutation the row is appended to DIR/results.json. With
--workdir DIR --resume a restarted run skips mutations that already have a row (the tree is
re-copied from the source and every file is touched, so objects built from an interrupted
mutant are always rebuilt).
"""
import argparse, json, os, platform, shutil, subprocess, sys, tempfile, time
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.abspath(os.path.join(HERE, "..", ".."))
COPY = ["CMakeLists.txt", "cmake", "src", "app", "plugins", "resources", "testsupport", "tests", "tools", "benchmarks"]


def run(cmd, cwd, check=False):
    p = subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), capture_output=True, text=True)
    if check and p.returncode != 0:
        sys.stderr.write(p.stdout[-3000:] + p.stderr[-3000:])
        raise SystemExit(f"command failed: {cmd}")
    return p


def failed_tests(build_dir):
    """Return (failed test names, ctest status, diagnostic).

    Missing or malformed JUnit output is infrastructure failure, never a green test run.
    """
    junit = os.path.join(build_dir, "junit.xml")
    if os.path.exists(junit):
        os.remove(junit)
    try:
        completed = run(["ctest", "--output-junit", "junit.xml", "--output-on-failure"], build_dir)
    except OSError as error:
        return None, None, f"ctest could not be launched ({type(error).__name__})"
    if not os.path.exists(junit):
        detail = "ctest did not produce a JUnit report"
        return None, completed.returncode, detail
    try:
        root = ET.parse(junit).getroot()
    except (ET.ParseError, OSError) as error:
        return None, completed.returncode, f"JUnit report could not be parsed ({type(error).__name__})"
    failed = set()
    for case in root.iter("testcase"):
        if case.find("failure") is not None or case.find("error") is not None or case.get("status") == "fail":
            failed.add(case.get("name"))
    return failed, completed.returncode, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", nargs="*", default=[])
    ap.add_argument("--report", default=os.path.join(SRC, "docs", "mutation-report.md"))
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--resume", action="store_true", help="skip mutations already recorded in WORKDIR/results.json")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    args = ap.parse_args()

    mutations = json.load(open(os.path.join(HERE, "mutations.json")))["mutations"]
    if args.only:
        mutations = [m for m in mutations if m["id"] in args.only]

    work = args.workdir or tempfile.mkdtemp(prefix="ar_mutation_")
    tree = os.path.join(work, "tree")
    if os.path.exists(tree):
        shutil.rmtree(tree)
    os.makedirs(tree)
    for item in COPY:
        src = os.path.join(SRC, item)
        dst = os.path.join(tree, item)
        (shutil.copytree if os.path.isdir(src) else shutil.copy2)(src, dst)
    now = time.time()
    for dirpath, _, names in os.walk(tree):          # force a rebuild of anything compiled from an old mutant
        for n in names:
            os.utime(os.path.join(dirpath, n), (now, now))
    build = os.path.join(work, "build")
    os.makedirs(work, exist_ok=True)
    results_path = os.path.join(work, "results.json")
    done = {}
    if args.resume and os.path.exists(results_path):
        done = {r["id"]: r for r in json.load(open(results_path))}
        print(f"[mutation] resuming: {len(done)} mutation(s) already recorded")

    print(f"[mutation] scratch tree: {tree}")
    run(["cmake", "-S", tree, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DAR_BUILD_TOOLS=OFF"], work, check=True)
    run(["cmake", "--build", build, "-j", str(args.jobs)], work, check=True)
    baseline, baseline_rc, baseline_error = failed_tests(build)
    if baseline_error or baseline_rc != 0 or baseline:
        raise SystemExit(
            f"baseline is not green (exit={baseline_rc}, failing={sorted(baseline or [])}, "
            f"diagnostic={baseline_error}); refusing to mutate")
    print("[mutation] baseline green")

    rows = []
    for m in mutations:
        if m["id"] in done:
            rows.append(done[m["id"]])
            print(f"[mutation] {m['id']}: {done[m['id']]['outcome']} (recorded earlier)")
            continue
        path = os.path.join(tree, m["file"])
        original = open(path, encoding="utf-8").read()
        count = original.count(m["old"])
        row = dict(id=m["id"], description=m["description"], file=m["file"],
                   expected=m["expected_failures"], failed=[], outcome="")
        if count != 1:
            row["outcome"] = "STALE"
            rows.append(row)
            json.dump(rows, open(results_path, "w"), indent=2)
            print(f"[mutation] {m['id']}: STALE ({count} matches)")
            continue
        open(path, "w", encoding="utf-8").write(original.replace(m["old"], m["new"], 1))
        try:
            built = run(["cmake", "--build", build, "-j", str(args.jobs)], work)
            if built.returncode != 0:
                row["outcome"] = "BUILD-ERROR"
            else:
                failed, test_rc, test_error = failed_tests(build)
                row["failed"] = sorted(failed or [])
                if test_error or (test_rc != 0 and not failed):
                    row["outcome"] = "TEST-ERROR"
                    row["detail"] = test_error or f"ctest exited {test_rc} without parsed failed test cases"
                elif not failed and test_rc == 0:
                    row["outcome"] = "SURVIVED"
                elif set(m["expected_failures"]) == failed:
                    row["outcome"] = "KILLED"
                else:
                    row["outcome"] = "KILLED-OTHER"
        finally:
            open(path, "w", encoding="utf-8").write(original)
        rows.append(row)
        json.dump(rows, open(results_path, "w"))
        print(f"[mutation] {m['id']}: {row['outcome']}  failing={row['failed']}")

    # leave the scratch build consistent with the pristine tree
    run(["cmake", "--build", build, "-j", str(args.jobs)], work)

    cmake_v = run(["cmake", "--version"], work).stdout.splitlines()[0]
    cxx = run("c++ --version", work).stdout.splitlines()[0]
    lines = ["# Mutation report", "",
             f"Generated {time.strftime('%Y-%m-%d %H:%M:%S %Z')} on {platform.platform()}",
             f"{cmake_v}; {cxx}", "",
             "| ID | Defect injected | Outcome | Expected to fail | Actually failed |", "|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['id']} | {r['description']} | **{r['outcome']}** | {', '.join(r['expected'])} | {', '.join(r['failed']) or '-'} |")
    killed = sum(r["outcome"] == "KILLED" for r in rows)
    lines += ["", f"**{killed} of {len(rows)} mutations killed as expected.**"]
    open(args.report, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(f"[mutation] report: {args.report}")
    if not args.keep and not args.workdir:
        shutil.rmtree(work, ignore_errors=True)
    return 0 if killed == len(rows) else 1


if __name__ == "__main__":
    sys.exit(main())
