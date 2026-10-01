#!/usr/bin/env python3
# asmfn UNIT FUNC : print the body of FUNC from build/HAGE/asm/<UNIT>.s as MWCC inline asm
# (for `asm` functions in C files; the repo must not contain .s files).
# Example: asmfn.py revolution/VF/pf_dir VFiPFDIR_DoMakeDir
# Labels become lbl_<addr>; branch targets and relocations are kept symbolic.
# Small-data references (@sda21) and data labels must be declared in the C file.
import sys, re, os
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
unit, func = sys.argv[1], sys.argv[2]
lines = open(f"build/HAGE/asm/{unit}.s").read().split("\n")
out = []
inside = False
for l in lines:
    if re.match(rf"\.fn {re.escape(func)},", l):
        inside = True
        continue
    if not inside:
        continue
    if l.startswith(".endfn"):
        break
    m = re.match(r"^\.L_([0-9A-F]+):", l)
    if m:
        out.append(f"lbl_{m.group(1)}:")
        continue
    m = re.match(r"^/\* [0-9A-F]{8} [0-9A-F]{8}  [0-9A-F ]{11} \*/\t(.*)$", l)
    if m:
        ins = m.group(1)
        ins = re.sub(r"\.L_([0-9A-F]+)", r"lbl_\1", ins)
        out.append("    " + ins)
print("    nofralloc")
print("\n".join(out))
