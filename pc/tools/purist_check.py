#!/usr/bin/env python3
"""Check that purist mode still draws exactly the same frames.

Every PC enhancement must leave `--purist` untouched (docs/pc_port.md, R13).
This runs the game in purist mode on a scripted input with a fixed clock,
hashes the frames it presents, and compares them with a recorded baseline.

    pc/tools/purist_check.py --record     # before changing anything: write the baseline
    pc/tools/purist_check.py              # afterwards: compare with it (exit 1 on a difference)

The baseline (build/pc/purist_baseline.json) holds hashes only, and is tied to
this machine's WAD contents, news files and OpenGL driver: record and compare
on the same machine. Screenshots go to build/scratch/purist and are not
committed.

Options: --binary FILE, --news-dir DIR (default: none, the run ends on the
connection error screen), --date ISO (default 2026-01-01T12:00Z).
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BASELINE = os.path.join(ROOT, "build", "pc", "purist_baseline.json")
SHOTS = os.path.join(ROOT, "build", "scratch", "purist")
FRAMES = [20, 200, 290, 350, 600]
INPUT = "P0:0@1,A@300"


def run(args):
    shutil.rmtree(SHOTS, ignore_errors=True)
    os.makedirs(os.path.join(SHOTS, "nand"))
    cmd = [
        args.binary, "--boot", "--purist", "--no-window", "--mute",
        "--nand-dir", os.path.join(SHOTS, "nand"),
        "--date", args.date,
        "--frames", str(max(FRAMES) + 20),
        "--input", INPUT,
        "--screenshot", ",".join(str(f) for f in FRAMES),
        "--screenshot-dir", SHOTS,
    ]
    # Without --news-dir the game must not find the default news directory.
    cmd += ["--news-dir", args.news_dir if args.news_dir else os.path.join(SHOTS, "no-news")]
    with open(os.path.join(SHOTS, "run.log"), "w") as log:
        status = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT).returncode
    if status != 0:
        sys.exit(f"newschannel exited with status {status}; see {SHOTS}/run.log")
    hashes = {}
    for frame in FRAMES:
        path = os.path.join(SHOTS, f"frame_{frame:06d}.png")
        if not os.path.exists(path):
            sys.exit(f"missing screenshot {path}")
        with open(path, "rb") as f:
            hashes[str(frame)] = hashlib.sha256(f.read()).hexdigest()
    return hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--record", action="store_true")
    parser.add_argument("--binary", default=os.path.join(ROOT, "build", "pc", "newschannel"))
    parser.add_argument("--news-dir", default="")
    parser.add_argument("--date", default="2026-01-01T12:00Z")
    args = parser.parse_args()

    key = {"news_dir": args.news_dir, "date": args.date, "input": INPUT}
    hashes = run(args)
    if args.record:
        with open(BASELINE, "w") as f:
            json.dump({"key": key, "frames": hashes}, f, indent=1)
        print(f"recorded {len(hashes)} frames in {os.path.relpath(BASELINE, ROOT)}")
        return 0
    if not os.path.exists(BASELINE):
        sys.exit("no baseline: run with --record first")
    with open(BASELINE) as f:
        baseline = json.load(f)
    if baseline["key"] != key:
        sys.exit("the baseline was recorded with other options; record again")
    different = [frame for frame in hashes if baseline["frames"].get(frame) != hashes[frame]]
    if different:
        print("purist mode changed: frames " + ", ".join(different) + f" differ (pictures in {os.path.relpath(SHOTS, ROOT)})")
        return 1
    print(f"purist mode unchanged: {len(hashes)} frames identical to the baseline")
    return 0


if __name__ == "__main__":
    sys.exit(main())
