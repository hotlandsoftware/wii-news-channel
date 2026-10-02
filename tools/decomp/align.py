#!/usr/bin/env python3
# align <unit> <start> <end> [--apply]
#
# Aligns the .text functions of a compiled unit (build/HAGE/src/<unit>.o, in
# section order) with the functions in [start, end) of config/HAGE/symbols.txt
# by size (longest common subsequence), and prints the renames that would give
# the DOL functions our mangled names. With --apply, runs them through ren.py
# (only symbols still named fn_XXXXXXXX are renamed; static functions get
# :local, weak ones keep their scope).
import os, re, subprocess, sys

os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
unit, start, end = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
apply = "--apply" in sys.argv

obj = f"build/HAGE/src/{unit}.o"
nm = subprocess.run(["build/binutils/powerpc-eabi-nm", "-S", "-n", "--defined-only", obj],
                    capture_output=True, text=True).stdout
ours = []
for l in nm.split("\n"):
    p = l.split()
    if len(p) == 4 and p[2] in "TtWw":
        ours.append((p[3], int(p[1], 16), p[2]))
# nm -n sorts by address only within all sections; keep .text ones (T/t/W)

dol = []
for l in open("config/HAGE/symbols.txt"):
    m = re.match(r"(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)", l)
    if m:
        a = int(m.group(2), 16)
        if start <= a < end:
            dol.append((m.group(1), a, int(m.group(3), 16)))
dol.sort(key=lambda x: x[1])

n, m = len(ours), len(dol)
dp = [[0] * (m + 1) for _ in range(n + 1)]
for i in range(n - 1, -1, -1):
    for j in range(m - 1, -1, -1):
        if ours[i][1] == dol[j][2]:
            dp[i][j] = 1 + dp[i + 1][j + 1]
        else:
            dp[i][j] = max(dp[i + 1][j], dp[i][j + 1])
i = j = 0
pairs = []
while i < n and j < m:
    if ours[i][1] == dol[j][2] and dp[i][j] == 1 + dp[i + 1][j + 1]:
        pairs.append((ours[i], dol[j]))
        i += 1
        j += 1
    elif dp[i + 1][j] >= dp[i][j + 1]:
        print(f"  ours only: {ours[i][0]} 0x{ours[i][1]:X}")
        i += 1
    else:
        print(f"  dol only:  {dol[j][0]} @{dol[j][1]:08X} 0x{dol[j][2]:X}")
        j += 1
for k in range(i, n):
    print(f"  ours only: {ours[k][0]} 0x{ours[k][1]:X}")
for k in range(j, m):
    print(f"  dol only:  {dol[k][0]} @{dol[k][1]:08X} 0x{dol[k][2]:X}")

args = []
for (name, size, kind), (dname, addr, dsize) in pairs:
    mark = "" if dname == name else "  <-"
    print(f"{addr:08X} {size:5X} {dname:24s} {name}{mark}")
    if dname != name and re.match(r"fn_[0-9A-F]{8}$", dname):
        args.append(f"{dname}={name}" + (":local" if kind == "t" else ""))
if apply and args:
    subprocess.run([sys.executable, "tools/decomp/ren.py"] + args)
