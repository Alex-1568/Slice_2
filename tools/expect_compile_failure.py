#!/usr/bin/env python3
"""A compile-fail regression: succeed only when the supplied attack is rejected because the
IRandomProvider copy/move operation is inaccessible or deleted. Dependency-free Python stdlib.
"""
import argparse
import os
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--include-dir", required=True)
    args = parser.parse_args()

    compiler_name = os.path.basename(args.compiler).lower()
    is_msvc = compiler_name in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}
    with tempfile.TemporaryDirectory(prefix="ar_compile_fail_") as temp:
        if is_msvc:
            output = os.path.join(temp, "probe.obj")
            command = [args.compiler, "/nologo", "/std:c++20", "/c", args.source,
                       "/I" + args.include_dir, "/Fo" + output]
        else:
            command = [args.compiler, "-std=c++20", "-fsyntax-only", "-I", args.include_dir, args.source]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired) as error:
            print(f"FAIL: could not run compile-fail probe: {error}")
            return 1

        diagnostic = result.stdout + "\n" + result.stderr
        if result.returncode == 0:
            print("FAIL: forbidden IRandomProvider base copy operation compiled successfully")
            return 1
        lowered = diagnostic.lower()
        mentions_interface = "irandomprovider" in lowered
        indicates_copy_access = any(token in lowered for token in (
            "deleted function", "deleted copy", "deleted constructor", "deleted", "c2280", "inaccessible", "private within this context",
            "protected within this context", "is protected", "no matching constructor",
        ))
        if not (mentions_interface and indicates_copy_access):
            print("FAIL: compilation failed for an unrelated or unrecognized reason")
            print(diagnostic[-5000:])
            return 1
        print("PASS: forbidden IRandomProvider copy operation is rejected by the compiler")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
