#!/usr/bin/env python3
"""Resolve git merge conflicts in config/HAGE/symbols.txt.

For every address in a conflict hunk, keep the side that has a real name over
an auto-generated one (fn_/lbl_/jumptable_/gap_/@...). If both sides name the
symbol differently, keep ours (HEAD) and report it.
"""
import os
import re
import sys

os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
path = "config/HAGE/symbols.txt"
text = open(path).read()
AUTO = re.compile(r"^(fn_|lbl_|jumptable_|gap_|@|dtor_|func_)")
LINE = re.compile(r"^(\S+) = (\S+):0x([0-9A-Fa-f]+);")


def parse(block):
    entries = []
    for line in block.splitlines():
        m = LINE.match(line)
        key = (m.group(2), int(m.group(3), 16)) if m else None
        entries.append((key, m.group(1) if m else None, line))
    return entries


def resolve(match):
    ours = parse(match.group(1))
    theirs = parse(match.group(2))
    by_key = {}
    order = []
    for key, name, line in ours + theirs:
        if key is None:
            continue
        if key not in by_key:
            by_key[key] = (name, line)
            order.append(key)
            continue
        old_name, old_line = by_key[key]
        if old_name == name:
            continue
        if AUTO.match(old_name) and not AUTO.match(name):
            by_key[key] = (name, line)
        elif not AUTO.match(old_name) and not AUTO.match(name):
            print(f"both named at {key[0]}:{key[1]:#x}: keeping {old_name}, dropping {name}", file=sys.stderr)
    order.sort(key=lambda k: (k[0], k[1]))
    return "".join(by_key[k][1] + "\n" for k in order)


pattern = re.compile(r"<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", re.S)
text, n = pattern.subn(resolve, text)
open(path, "w").write(text)
print(f"resolved {n} hunks", file=sys.stderr)
