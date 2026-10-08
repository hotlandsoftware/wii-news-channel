#!/usr/bin/env python3
"""Compile a candidate version of a source file outside the tree and compare one function
with the original object.

Usage: fnscore.py <src> <mangled symbol> [candidate file] [-q]

Without a candidate file the source in the tree is scored. Prints the differing instructions
(original | candidate) and (differing instructions, candidate length, original length).

As a module:
    from fnscore import Scorer, disasm, subst
    s = Scorer("src/news/NewsArticle.cpp", "GetPicture__8NewsDataFP14NewsTextBuffer")
    n, ours, orig = s.score(candidate_text)      # compiled in a scratch directory, cached
    disasm(os.path.join(s.dir, "NewsArticle.o"), s.sym)   # the candidate's instructions
Run ninja once first: the original object comes from build/HAGE/obj.
"""
import os, re, subprocess, sys, hashlib, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCR = os.path.join(tempfile.gettempdir(), "fnscore-%d" % os.getuid())
OBJDUMP = ROOT + "/build/binutils/powerpc-eabi-objdump"


def disasm(obj, sym):
    out = subprocess.run([OBJDUMP, "-dr", "--no-show-raw-insn", obj], capture_output=True, text=True).stdout
    lines = out.split("\n")
    res = []
    on = False
    base = 0
    for l in lines:
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", l)
        if m:
            if on:
                break
            on = (m.group(2) == sym)
            base = int(m.group(1), 16)
            continue
        if not on:
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+(R_PPC_\S+)\s+(\S+)", l)
        if m:
            ins = res[-1]
            ins = re.sub(r"-?(0x)?[0-9a-f]+(\(r\d+\))$", r"REL\2", ins)
            ins = re.sub(r"(,)-?\d+$", r"\1REL", ins)
            ins = re.sub(r"^(bl?)\s.*$", r"\1 REL", ins)
            res[-1] = ins + " {" + m.group(2) + "}"
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+(.*)$", l)
        if m:
            ins = m.group(2).strip()
            ins = re.sub(r"\s+", " ", ins)

            def rel(mm):
                return "@%x" % (int(mm.group(1), 16) - base)
            ins = re.sub(r"\b([0-9a-f]+) <[^>]+>", rel, ins)
            res.append(ins)
    return res


class Scorer:
    def __init__(self, src, sym, tag="a"):
        self.src = src
        self.sym = sym
        rel = src[len("src/"):].rsplit(".", 1)[0]
        self.rel = rel
        out = subprocess.run(["ninja", "-t", "commands", "build/HAGE/src/" + rel + ".o"], capture_output=True,
                             text=True, cwd=ROOT).stdout
        self.cmd = out.strip().split("\n")[-1].split(" && ")[0]
        self.dir = os.path.join(SCR, "w_" + tag + "_" + os.path.basename(rel))
        os.makedirs(self.dir, exist_ok=True)
        self.base = os.path.basename(src)
        self.orig = disasm(ROOT + "/build/HAGE/obj/" + rel + ".o", sym)
        assert self.orig, "orig symbol not found"
        self.cache = {}

    def compile(self, text):
        p = os.path.join(self.dir, self.base)
        open(p, "w").write(text)
        cmd = self.cmd.replace("-MMD ", "")
        cmd = re.sub(r"-c \S+ -o \S+", "-c %s -o %s" % (p, self.dir), cmd)
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=ROOT)
        if r.returncode:
            return None, r.stdout + r.stderr
        return os.path.join(self.dir, os.path.splitext(self.base)[0] + ".o"), ""

    def score(self, text, show=False):
        h = hashlib.md5(text.encode()).hexdigest()
        if h in self.cache and not show:
            return self.cache[h]
        obj, err = self.compile(text)
        if obj is None:
            if show:
                print(err[-600:])
            return (9999, 0, len(self.orig))
        ours = disasm(obj, self.sym)
        n = sum(1 for a, b in zip(ours, self.orig) if a != b) + abs(len(ours) - len(self.orig))
        if show:
            for i, (a, b) in enumerate(zip(self.orig, ours)):
                if a != b:
                    print("%4x  %-40s | %s" % (i * 4, a, b))
        r = (n, len(ours), len(self.orig))
        self.cache[h] = r
        return r


def subst(text, pairs):
    for a, b in pairs:
        assert a in text, "not found: " + a[:60]
        text = text.replace(a, b, 1)
    return text


if __name__ == "__main__":
    src, sym = sys.argv[1:3]
    s = Scorer(src, sym)
    text = open(sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].startswith("-") else ROOT + "/" + src).read()
    print(s.score(text, show="-q" not in sys.argv))
