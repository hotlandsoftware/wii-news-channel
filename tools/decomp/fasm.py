#!/usr/bin/env python3
# fasm <name-or-addr> [count]: print clean asm for a function (searches build/HAGE/asm)
import sys, glob, re, os
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
key=sys.argv[1]; cnt=int(sys.argv[2]) if len(sys.argv)>2 else 1
pat=re.compile(r"^/\* ([0-9A-F]{8}) [0-9A-F]{8}  [0-9A-F ]{11} \*/\t(.*)$")
for f in sorted(glob.glob("build/HAGE/asm/**/*.s",recursive=True)):
    lines=open(f).read().split("\n")
    for i,l in enumerate(lines):
        m=re.match(r"^\.fn (\S+),",l)
        if m and (m.group(1)==key or m.group(1).endswith(key.upper()) or key in m.group(1)):
            n=0; j=i
            while j<len(lines) and n<cnt:
                l2=lines[j]; mm=pat.match(l2)
                if mm: print(f"{mm.group(1)[-4:]}  {mm.group(2)}")
                elif l2.startswith(".fn") or l2.startswith(".L") or l2.startswith("# .text"): print(l2)
                elif l2.startswith(".endfn"): n+=1; print(l2); print()
                elif l2.startswith(".section") or l2.startswith(".obj"): break
                j+=1
            sys.exit(0)
print("not found")
