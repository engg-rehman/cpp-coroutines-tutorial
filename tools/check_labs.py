#!/usr/bin/env python3
"""Compile every checkpoint; check intended diagnostics and exact runtime output."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lesson", nargs="?", help="lesson number, e.g. 01; default: all")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    cases = json.loads((ROOT / "labs/checkpoints.json").read_text())
    if args.lesson:
        cases = [c for c in cases if c["source"].startswith(f"labs/{args.lesson.zfill(2)}/")]
    if not cases:
        parser.error("no checkpoints selected")
    failures = 0
    compiled = {}
    flags = ["-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-O0", "-g"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="coroutine-labs-") as directory:
        for index, case in enumerate(cases):
            source = case["source"]
            name = case.get("name", source)
            try:
                if source not in compiled:
                    binary = str(Path(directory) / f"checkpoint-{index}")
                    build = subprocess.run(
                        shlex.split(args.cxx) + flags + [source, "-o", binary],
                        cwd=ROOT, capture_output=True, text=True, timeout=60)
                    compiled[source] = (build, binary)
                build, binary = compiled[source]
                if "error" in case:
                    if build.returncode == 0 or not re.search(case["error"], build.stderr, re.I):
                        raise RuntimeError("expected a compiler rejection mentioning " + case["error"] + "\n" + build.stderr)
                    label = "expected compile failure"
                else:
                    if build.returncode:
                        raise RuntimeError(build.stderr)
                    run = subprocess.run([binary] + case.get("args", []), cwd=ROOT,
                                         capture_output=True, text=True, timeout=10)
                    if run.returncode != case.get("exit", 0):
                        raise RuntimeError(f"exit {run.returncode}\n{run.stderr}")
                    if run.stdout != case["stdout"] or run.stderr != case.get("stderr", ""):
                        raise RuntimeError(f"expected stdout {case['stdout']!r}; got {run.stdout!r}\nstderr: {run.stderr}")
                    label = "output matched"
                print(f"PASS {name}: {label}")
            except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
                failures += 1
                print(f"FAIL {name}: {error}", file=sys.stderr)
    print(f"{len(cases) - failures}/{len(cases)} checkpoints passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
