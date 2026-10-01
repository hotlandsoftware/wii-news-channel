#!/usr/bin/env python3
# refcmp REF SRC START END [--mw VER] [-- extra cflags...]
#
# Quick check of how directly a file from another decomp ports to this DOL.
# Compiles SRC (a path inside the reference repo) with the reference's include paths,
# then matches each compiled function to a same-size function in [START, END) of our
# DOL and compares the code with relocated instructions masked out.
#
# REF is one of: ogws (doldecomp/ogws), smg (SMGCommunity/Petari), tp (zeldaret/tp).
# Reference repos are looked up in $DECOMP_REFS (default: ../decomp-refs next to the repo).
# Example:
#   refcmp.py smg src/RVL_SDK/arc/arc.c 0x8008A0A4 0x8008AA44 -- -ipa file -inline auto,level=3
import sys, os, re, struct, subprocess, tempfile

os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
ROOT = os.getcwd()
REFS = os.environ.get("DECOMP_REFS", os.path.normpath(os.path.join(ROOT, "..", "decomp-refs")))
if not os.path.isdir(REFS):
    REFS = "/home/admin/decomp-refs"

REF = {
    "ogws": ("ogws", ["-DVERSION_RSPE01_01", "-DREVOLUTION", "-i", "include", "-i", "include/MSL", "-i", "include/MSL/internal",
                      "-ir", "include/revolution/BTE", "-i", "include/nw4r", "-i", "include/homeButtonMiniLib"]),
    "smg": ("petari", ["-DREVOLUTION", "-i", "libs/MSL_C++/include", "-i", "libs/MSL_C/include", "-i", "libs/MetroTRK/include",
                       "-i", "libs/RVL_SDK/include", "-i", "libs/Runtime/include", "-i", "libs/nw4r/include",
                       "-i", "libs/RVLFaceLib/include", "-ir", "libs/RVL_SDK/include/revolution/bte"]),
    "tp": ("tp", ["-D__GEKKO__", "-DVERSION=7", "-D__REVOLUTION_SDK__", "-DSDK_SEP2006", "-i", "include", "-i", "src",
                  "-i", "libs/JSystem/include", "-i", "libs/revolution/include", "-i", "libs/revolution/include/revolution",
                  "-i", "libs/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Include",
                  "-i", "libs/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Include",
                  "-i", "libs/PowerPC_EABI_Support/MSL/MSL_C/PPC_EABI/Include",
                  "-i", "libs/PowerPC_EABI_Support/MSL/MSL_C++/MSL_Common/Include",
                  "-i", "libs/PowerPC_EABI_Support/Runtime/Inc", "-ir", "libs/revolution/src"]),
}
BASE = ["-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int", "-fp", "hardware", "-Cpp_exceptions", "off",
        "-O4,p", "-inline", "auto", "-pragma", "cats off", "-pragma", "warn_notinlined off", "-maxerrors", "1", "-nosyspath",
        "-RTTI", "off", "-str", "reuse", "-enc", "SJIS", "-DNDEBUG=1"]

args = sys.argv[1:]
extra = []
if "--" in args:
    i = args.index("--")
    args, extra = args[:i], args[i + 1:]
mw = "GC/3.0a5.2"
if "--mw" in args:
    i = args.index("--mw")
    mw = args[i + 1]
    del args[i:i + 2]
ref, src, start, end = args[0], args[1], int(args[2], 16), int(args[3], 16)
repo, inc = REF[ref]
repo = os.path.join(REFS, repo)

objdump = None
for cand in ["build/binutils/powerpc-eabi-objdump", "../../../build/binutils/powerpc-eabi-objdump"]:
    if os.path.exists(cand):
        objdump = os.path.abspath(cand)
        break
if objdump is None:
    sys.exit("powerpc-eabi-objdump not found (run ninja once so build/binutils exists)")

out = tempfile.mktemp(suffix=".o")
cmd = [os.path.join(ROOT, "build/tools/wibo"), os.path.join(ROOT, f"build/compilers/{mw}/mwcceppc.exe")] + BASE + extra + inc + ["-c", src, "-o", out]
r = subprocess.run(cmd, cwd=repo, capture_output=True, text=True)
if r.returncode:
    sys.exit("compile error:\n" + "\n".join((r.stdout + r.stderr).split("\n")[:20]))
od = subprocess.run([objdump, "-dr", "--section=.text", out], capture_output=True, text=True).stdout
os.unlink(out)

funcs = []
cur = None
for l in od.split("\n"):
    m = re.match(r"^([0-9a-f]+) <(.+)>:$", l)
    if m:
        cur = [m.group(2), int(m.group(1), 16), [], set()]
        funcs.append(cur)
        continue
    m = re.match(r"^\s+([0-9a-f]+):\s+([0-9a-f]{2}) ([0-9a-f]{2}) ([0-9a-f]{2}) ([0-9a-f]{2})", l)
    if m and cur:
        cur[2].append(int("".join(m.group(2, 3, 4, 5)), 16))
        continue
    m = re.match(r"^\s+([0-9a-f]+): R_PPC", l)
    if m and cur:
        cur[3].add((int(m.group(1), 16) - cur[1]) // 4)

dol = open("orig/HAGE/sys/main.dol", "rb").read()
offs = struct.unpack(">18I", dol[0:72]); addrs = struct.unpack(">18I", dol[72:144]); sizes = struct.unpack(">18I", dol[144:216])
def word(a):
    for i in range(18):
        if sizes[i] and addrs[i] <= a < addrs[i] + sizes[i]:
            o = offs[i] + a - addrs[i]
            return struct.unpack(">I", dol[o:o + 4])[0]
    return 0

ours = []
for line in open("config/HAGE/symbols.txt"):
    m = re.match(r"^(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)", line)
    if m and start <= int(m.group(2), 16) < end:
        ours.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))

def score(words, rel, a):
    return sum(1 for i, w in enumerate(words) if i in rel or word(a + 4 * i) == w or (w >> 26) == (word(a + 4 * i) >> 26) == 18)

used = set()
tot = ok = 0
for name, off, words, rel in funcs:
    n = len(words)
    best = (0, None)
    for a, sz, nm in ours:
        if a not in used and (nm == name or sz == 4 * n):
            sc = score(words, rel, a) + (n if nm == name else 0)
            if sc > best[0]:
                best = (sc, a)
    tot += n
    if best[1] is None:
        print(f"  --------  {4*n:5X}  {name}  (no same-size function)")
        continue
    used.add(best[1])
    s = score(words, rel, best[1])
    ok += s
    print(f"  {best[1]:08X}  {4*n:5X}  {100*s/n:5.1f}%  {name}")
rest = [f"{a:08X}+{sz:X}" for a, sz, nm in ours if a not in used]
print(f"  our functions without a counterpart: {len(rest)}")
print(f"TOTAL {100*ok/max(1,tot):.1f}% of 0x{4*tot:X} bytes")
