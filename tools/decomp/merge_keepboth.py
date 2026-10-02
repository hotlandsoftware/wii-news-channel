#!/usr/bin/env python3
"""Resolve merge conflicts by keeping both sides (ours first, then theirs).

For append-style files where parallel branches add independent entries:
config/HAGE/splits.txt and the docs. Don't use it on code.

Usage: tools/decomp/merge_keepboth.py FILE...
"""
import re
import sys

pat = re.compile(r"<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", re.S)


def keep_both(m):
    ours, theirs = m.group(1), m.group(2)
    if ours.strip() and theirs.strip():
        return ours.rstrip("\n") + "\n\n" + theirs
    return ours + theirs


for path in sys.argv[1:]:
    s = open(path).read()
    open(path, "w").write(pat.sub(keep_both, s))
