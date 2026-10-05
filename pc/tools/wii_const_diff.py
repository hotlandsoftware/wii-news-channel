#!/usr/bin/env python3
"""Compare the floating-point constants that the functions of a unit load.

objdiff's match percentage compares instructions and the names of the symbols
they refer to. A float or double literal is an anonymous symbol in a constant
pool, so a function can be reported as 100% while it loads 4.0f where the
original loads 30.0f. This tool finds those: for every function that is in
both the original object (build/HAGE/obj) and the compiled one
(build/HAGE/src) it lists the value behind each lfs/lfd and prints the
functions whose lists differ.

The same goes for pointers in data, above all pointers to member functions
(`IsState(&SlideShow::StateZoom)` is a 12-byte constant in .data): the tool
also compares, per data section, the symbols the data points to, in order.

    pc/tools/wii_const_diff.py                 # every unit that has both objects
    pc/tools/wii_const_diff.py news/Globe      # some units
    pc/tools/wii_const_diff.py -v news/Globe   # also print matching functions

Needs a finished Wii build (`ninja`). Prints values only for functions that
differ; exit status 1 if there are any.
"""

import difflib
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OBJ = os.path.join(ROOT, "build", "HAGE", "obj")
SRC = os.path.join(ROOT, "build", "HAGE", "src")

SHT_SYMTAB, SHT_RELA, SHT_NOBITS = 2, 4, 8
STT_FUNC = 2
OP_LFS, OP_LFSU, OP_LFD, OP_LFDU = 48, 49, 50, 51


class Elf:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.d = d = f.read()
        if d[:6] != b"\x7fELF\x01\x02":
            raise ValueError("%s: not a big-endian ELF32 file" % path)
        shoff, = struct.unpack_from(">I", d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from(">HHH", d, 0x2E)
        self.sections = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
                ">10I", d, shoff + i * shentsize)
            self.sections.append(dict(name=name, type=typ, off=off, size=size, link=link, info=info))
        strs = self.sections[shstrndx]
        for s in self.sections:
            s["name"] = self._str(strs, s["name"])
        self.symbols = []
        self.relocs = {}  # section index -> {offset: (symbol index, addend)}
        for s in self.sections:
            if s["type"] == SHT_SYMTAB:
                strtab = self.sections[s["link"]]
                for o in range(s["off"], s["off"] + s["size"], 16):
                    name, value, size, info, other, shndx = struct.unpack_from(">IIIBBH", d, o)
                    self.symbols.append(dict(name=self._str(strtab, name), value=value, size=size,
                                             type=info & 15, shndx=shndx))
        for s in self.sections:
            if s["type"] == SHT_RELA:
                table = self.relocs.setdefault(s["info"], {})
                for o in range(s["off"], s["off"] + s["size"], 12):
                    offset, info, addend = struct.unpack_from(">IIi", d, o)
                    table[offset & ~3] = (info >> 8, addend)

    def _str(self, strtab, offset):
        start = strtab["off"] + offset
        return self.d[start:self.d.index(b"\0", start)].decode("latin-1")

    def functions(self):
        out = {}
        for sym in self.symbols:
            if sym["type"] == STT_FUNC and 0 < sym["shndx"] < len(self.sections) and sym["size"]:
                out[sym["name"]] = sym
        return out

    def data_pointers(self):
        """The symbols that initialised data points to, per section, in order.

        Pointers to member functions and tables of them are data: a function
        can match while the state it compares with is a different one."""
        out = {}
        for index, sec in enumerate(self.sections):
            if not sec["name"].startswith((".data", ".sdata", ".rodata")) or index not in self.relocs:
                continue
            names = []
            for offset in sorted(self.relocs[index]):
                sym = self.symbols[self.relocs[index][offset][0]]
                name = sym["name"]
                # switch tables and string pools have no stable names
                if sym["type"] == STT_FUNC or "__" in name:
                    names.append(name)
            out[sec["name"]] = names
        return out

    def constants(self, func):
        """The values loaded by lfs/lfd through a relocation, in code order."""
        sec = self.sections[func["shndx"]]
        relocs = self.relocs.get(func["shndx"], {})
        out = []
        for offset in range(func["value"], func["value"] + func["size"], 4):
            if offset not in relocs:
                continue
            word, = struct.unpack_from(">I", self.d, sec["off"] + offset)
            op = word >> 26
            if op not in (OP_LFS, OP_LFSU, OP_LFD, OP_LFDU):
                continue
            symidx, addend = relocs[offset]
            sym = self.symbols[symidx]
            if not 0 < sym["shndx"] < len(self.sections):
                out.append("extern %s" % sym["name"] if sym["name"].startswith("lbl_") is False else "extern")
                continue
            target = self.sections[sym["shndx"]]
            if target["type"] == SHT_NOBITS:
                out.append("bss")
                continue
            if not target["name"].startswith((".sdata2", ".rodata")):
                # a variable, not a literal: its initial value says nothing
                out.append("var")
                continue
            # A displacement in the instruction is part of the address for
            # relocations against a pool base (.rodata + lo16).
            at = target["off"] + sym["value"] + addend
            if op in (OP_LFS, OP_LFSU):
                out.append("%r" % struct.unpack_from(">f", self.d, at)[0])
            else:
                out.append("%r" % struct.unpack_from(">d", self.d, at)[0])
        return out


def compare(unit, verbose):
    a_path = os.path.join(OBJ, unit + ".o")
    b_path = os.path.join(SRC, unit + ".o")
    if not (os.path.exists(a_path) and os.path.exists(b_path)):
        return None
    a, b = Elf(a_path), Elf(b_path)
    fa, fb = a.functions(), b.functions()
    bad = 0
    for name in sorted(fa, key=lambda n: fa[n]["value"]):
        if name not in fb:
            continue
        ca, cb = a.constants(fa[name]), b.constants(fb[name])
        # externs and variables are named symbols, which objdiff does compare
        norm = lambda seq: [c for c in seq if c[0] in "-0123456789in"]
        na, nb = norm(ca), norm(cb)
        if na == nb:
            if verbose:
                print("  ok   %s (%d constants)" % (name, len(na)))
            continue
        bad += 1
        print("%s: %s" % (unit, name))
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, na, nb, autojunk=False).get_opcodes():
            if tag != "equal":
                print("    #%d original %s | source %s" % (i1, " ".join(na[i1:i2]) or "-", " ".join(nb[j1:j2]) or "-"))
    # Only for the game: library units differ in vtables of inline classes
    # and in switch tables, which is noise here.
    if not unit.startswith("news/"):
        return bad
    pa, pb = a.data_pointers(), b.data_pointers()
    for section in sorted(set(pa) | set(pb)):
        na, nb = pa.get(section, []), pb.get(section, [])
        if na == nb:
            continue
        bad += 1
        print("%s: pointers in %s (%d original, %d source)" % (unit, section, len(na), len(nb)))
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, na, nb, autojunk=False).get_opcodes():
            if tag != "equal":
                print("    #%d original %s | source %s" % (i1, " ".join(na[i1:i2]) or "-", " ".join(nb[j1:j2]) or "-"))
    return bad


def main():
    args = [a for a in sys.argv[1:] if a != "-v"]
    verbose = "-v" in sys.argv[1:]
    units = args
    if not units:
        for base, _, files in os.walk(SRC):
            for f in files:
                if f.endswith(".o"):
                    units.append(os.path.relpath(os.path.join(base, f), SRC)[:-2])
        units.sort()
    total = 0
    seen = 0
    for unit in units:
        n = compare(unit, verbose)
        if n is None:
            if args:
                print("%s: no pair of objects" % unit)
            continue
        seen += 1
        total += n
    print("%d function(s) or data section(s) differ in their constants (%d units compared)" % (total, seen))
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
