#!/usr/bin/env python3
"""Check that the game's classes have the Wii's layout in the PC build.

The headers in include/news note the Wii offset of each member
("s32 mCount;  // at 0x0F0"). Several source files do not include the real
header of a class they use; they declare their own view of it with those
offsets, or read a member through a cast. That only works on PC if gcc lays
the class out as CodeWarrior did, which it does not by itself: a pointer to
member function is 12 bytes for CodeWarrior and 8 for gcc, and gcc puts the
vtable pointer first where CodeWarrior puts it after the members of the class
that declares the first virtual function.

This tool turns every offset comment into an offsetof() check, compiles the
checks with the PC build's flags and runs them:

    pc/tools/layout_check.py            # prints the members that differ

Exit status 1 if any member is somewhere else than on the Wii. Fix a
difference in the header, under TARGET_PC (PC_PMF_PAD after a pointer to
member function, see include/pc/compat.h).
"""

import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

CLASS = re.compile(r"^(\s*)(class|struct|union)\s+([A-Za-z_0-9]+)\s*(:[^;{]*)?\{\s*(//.*)?$")
ANON = re.compile(r"^(\s*)(struct|union)\s*\{")
CLOSE = re.compile(r"^(\s*)\}[^;]*;")
MEMBER = re.compile(r"^\s*[^/(]*?[\s\*&]([A-Za-z_0-9]+)(\[[^\]]*\])*\s*;\s*//\s*(?:at\s+)?(0x[0-9A-Fa-f]+)")


def collect(header):
    checks = []
    stack = []
    with open(header) as f:
        for line in f:
            line = line.rstrip("\n")
            m = CLASS.match(line)
            if m:
                stack.append((m.group(3), len(m.group(1))))
                continue
            m = ANON.match(line)
            if m:
                stack.append((None, len(m.group(1))))
                continue
            m = CLOSE.match(line)
            if m and stack and stack[-1][1] == len(m.group(1)):
                stack.pop()
                continue
            if not stack or any(name is None for name, _ in stack):
                continue
            code = line.split("//")[0]
            m = MEMBER.match(line)
            if m and "static" not in code and "(" not in code:
                checks.append(("::".join(name for name, _ in stack), m.group(1), int(m.group(3), 16)))
    return checks


def main():
    # The flags of pc/cmake/NewsLibrary.cmake for the game's files.
    flags = [
        "-std=gnu++17", "-m32", "-fshort-wchar", "-msse2", "-mfpmath=sse",
        "-idirafter", os.path.join(ROOT, "include"),
        "-idirafter", os.path.join(ROOT, "build", "HAGE", "include"),
        "-include", os.path.join(ROOT, "include", "pc", "compat.h"),
        "-DTARGET_PC=1", "-DNDEBUG=1", "-DVERSION_HAGE",
        "-DNW4R_MATH_VEC2_NO_DTOR", "-DNW4R_MATH_VEC3_NO_DTOR", "-DNW4R_MATH_MTX34_NO_DTOR",
        "-DNW4R_UT_COLOR_DEFAULT_WHITE", "-DNW4R_UT_RECT_DEFAULT_ZERO",
        "-fpermissive", "-fno-access-control", "-fms-extensions", "-w",
    ]
    total = 0
    bad = 0
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        # One program per header: some headers hold their own view of a class
        # of another header, so they cannot be included together.
        for header in sorted(glob.glob(os.path.join(ROOT, "include", "news", "*.h"))):
            checks = collect(header)
            if not checks:
                continue
            rel = os.path.relpath(header, os.path.join(ROOT, "include"))
            lines = ["#include <cstddef>", "#include <cstdio>", "#include <%s>" % rel, "int main() {", "    int bad = 0;"]
            for cls, member, offset in checks:
                lines.append(
                    '    if (offsetof(%s, %s) != 0x%X) { bad++; std::printf("%s: %s::%s is at 0x%%X on PC, 0x%X on the Wii\\n",'
                    " (unsigned)offsetof(%s, %s)); }" % (cls, member, offset, rel, cls, member, offset, cls, member))
            lines.append("    return bad;\n}")
            src = os.path.join(tmp, "layout_check.cpp")
            exe = os.path.join(tmp, "layout_check")
            with open(src, "w") as f:
                f.write("\n".join(lines) + "\n")
            if subprocess.run(["c++"] + flags + [src, "-o", exe]).returncode != 0:
                print("%s: does not compile on its own" % rel)
                failed += 1
                continue
            total += len(checks)
            bad += subprocess.run([exe]).returncode
    print("%d of %d members are not at their Wii offset" % (bad, total))
    return 1 if bad or failed else 0


if __name__ == "__main__":
    sys.exit(main())
