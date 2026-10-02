#!/usr/bin/env python3
"""After a merge, re-apply every symbol the incoming branch changed.

For each symbols.txt line the branch added or changed relative to the merge
base, replace whatever HEAD's working copy has at the same section and
address with the branch's line. Lines the branch removed are removed too.
Use it when the branch owns an address range and its names, sizes and
alignment should win.

Usage: tools/decomp/symbols_take_theirs.py <branch> [config/HAGE/symbols.txt]
"""
import re
import subprocess
import sys

branch = sys.argv[1]
path = sys.argv[2] if len(sys.argv) > 2 else "config/HAGE/symbols.txt"
key_re = re.compile(r"^\S+ = (\.[\w.$]+:0x[0-9A-Fa-f]+);")


def show(rev):
    return subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True, text=True, check=True).stdout


def keyed(text):
    out = {}
    for line in text.splitlines():
        m = key_re.match(line)
        if m:
            out[m.group(1).upper()] = line
    return out


base_rev = subprocess.run(["git", "merge-base", "HEAD", branch], capture_output=True, text=True, check=True).stdout.strip()
base, theirs = keyed(show(base_rev)), keyed(show(branch))
changed = {k: v for k, v in theirs.items() if base.get(k) != v}
removed = {k for k in base if k not in theirs}

lines = open(path).read().split("\n")
out, seen = [], set()
for line in lines:
    m = key_re.match(line)
    k = m.group(1).upper() if m else None
    if k in changed:
        if k not in seen:
            out.append(changed[k])
            seen.add(k)
        continue
    if k in removed:
        continue
    out.append(line)
for k, v in changed.items():
    if k not in seen:
        out.append(v)
open(path, "w").write("\n".join(out))
print(f"applied {len(changed)} changed, removed {len(removed)} from {branch}")
