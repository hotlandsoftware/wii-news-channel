#!/usr/bin/env python3
"""Resolve a configure.py merge conflict between parallel library branches.

Usage: tools/decomp/merge_configure.py <their-branch>

Starts from HEAD's configure.py. Lib blocks that the branch added or changed
(relative to the merge base) are copied in from the branch. Non-lib lines the
branch added (e.g. new cflags variables) are reported, so they can be applied
by hand.
"""
import subprocess
import sys


def show(rev):
    return subprocess.run(["git", "show", f"{rev}:configure.py"], capture_output=True, text=True, check=True).stdout


def blocks(s):
    out = {}
    i = 0
    head = '    {\n        "lib": "'
    while True:
        i = s.find(head, i)
        if i < 0:
            break
        name = s[i + len(head):].split('"')[0]
        j = s.find("\n    },\n", i) + len("\n    },\n")
        out[name] = s[i:j]
        i = j
    return out


def strip_blocks(s):
    for b in blocks(s).values():
        s = s.replace(b, "")
    return s


theirs_rev = sys.argv[1]
base_rev = subprocess.run(["git", "merge-base", "HEAD", theirs_rev], capture_output=True, text=True, check=True).stdout.strip()
ours, theirs, base = show("HEAD"), show(theirs_rev), show(base_rev)
ob, tb, bb = blocks(ours), blocks(theirs), blocks(base)

last = list(ob.values())[-1]
for name, block in tb.items():
    if bb.get(name) == block:
        continue  # unchanged on their side
    if name in ob:
        if ob[name] != bb.get(name):
            print(f"warning: lib {name} changed on both sides; taking theirs", file=sys.stderr)
        ours = ours.replace(ob[name], block)
        print(f"updated lib {name}")
    else:
        ours = ours.replace(last, last + block)
        last = last + block
        print(f"added lib {name}")

base_rest = set(strip_blocks(base).splitlines())
ours_rest = set(strip_blocks(ours).splitlines())
for line in strip_blocks(theirs).splitlines():
    if line not in base_rest and line not in ours_rest:
        print(f"NOTE: non-lib line only on their side: {line!r}")

open("configure.py", "w").write(ours)
