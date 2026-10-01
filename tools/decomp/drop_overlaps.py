#!/usr/bin/env python3
"""Drop auto-named lbl_ symbols that overlap a real (named) symbol in symbols.txt.

Merging parallel branches can bring back dtk's placeholder `lbl_XXXXXXXX`
symbols after one branch has replaced them with real names and sizes.

Usage: tools/decomp/drop_overlaps.py [config/HAGE/symbols.txt]
"""
import re
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "config/HAGE/symbols.txt"
lines = open(path).read().split("\n")
pat = re.compile(r"^(\S+) = (\.[\w.$]+):0x([0-9A-Fa-f]+); // .*?size:0x([0-9A-Fa-f]+)")
syms = []
for i, l in enumerate(lines):
    m = pat.match(l)
    if m:
        name, sec, addr, size = m.group(1), m.group(2), int(m.group(3), 16), int(m.group(4), 16)
        syms.append((i, name, sec, addr, size))

named = [s for s in syms if not s[1].startswith("lbl_") and s[4] > 0]
drop = set()
for i, name, sec, addr, size in syms:
    if not name.startswith("lbl_"):
        continue
    end = addr + max(size, 1)
    for _, n2, s2, a2, z2 in named:
        if s2 == sec and a2 < end and addr < a2 + z2:
            drop.add(i)
            print(f"drop {name} ({sec} 0x{addr:08X}+0x{size:X}), overlaps {n2}")
            break

open(path, "w").write("\n".join(l for i, l in enumerate(lines) if i not in drop))
