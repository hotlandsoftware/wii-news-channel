#!/usr/bin/env python3
"""Random local search over semantics-preserving source edits of one C function.

Usage: srcsearch.py <src> <function> [iterations] [--restarts N]

<src> is a path like src/revolution/TMCC_JPEG/jpgd_dec.c. The unit is derived from it.
Each candidate is compiled directly with the unit's ninja compile command (ninja itself is
not run, so searches on different files can run in parallel) and scored with objdiff-cli.
The best version is written back only if it scores higher than the original.

Edits tried:
  - reorder the declarations at the top of the function (--restarts N also tries N random
    full shuffles first)
  - reorder the simple "x = expr;" setup statements after the declarations, keeping every
    statement after the ones whose results it reads or writes
  - swap the operands of a commutative operator when the operation is a whole
    parenthesised group or a whole right-hand side
  - swap two adjacent independent simple statements
  - toggle "x = x op y;" and "x op= y;"

Register allocation in MWCC depends on declaration order, statement order and which operand
of a commutative operation is evaluated first, so these edits often fix register swaps.
Read the result: the search can leave odd but equivalent code (e.g. "(3 & j)").
"""
import json
import os
import random
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)


def span(text, func):
    m = re.search(r"^[A-Za-z][^\n;]*\b" + re.escape(func) + r"\([^;{]*\)\s*\{", text, re.M)
    if not m:
        sys.exit("function not found: " + func)
    i = text.index("{", m.start())
    depth = 0
    for k in range(i, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return m.start(), k + 1


_cmd = {}


def score(src, func):
    rel = src[len("src/"):].rsplit(".", 1)[0]
    obj = "build/HAGE/src/" + rel + ".o"
    if obj not in _cmd:
        out = subprocess.run(["ninja", "-t", "commands", obj], capture_output=True, text=True).stdout
        _cmd[obj] = out.strip().split("\n")[-1].split(" && ")[0]
    r = subprocess.run(_cmd[obj], shell=True, capture_output=True, text=True)
    if r.returncode:
        return -1.0
    fd, path = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    subprocess.run(["build/tools/objdiff-cli", "diff", "-p", ".", "-u", "main/" + rel, "-o", path,
                    "--format", "json"], capture_output=True)
    try:
        d = json.load(open(path))
    finally:
        os.unlink(path)
    for s in d["left"]["symbols"]:
        if s["name"] == func:
            return float(s.get("match_percent") or 0.0)
    # C++: match the mangled name of a free function or member ("Func__..." or
    # "Func__5Class..."); pass the mangled name with --sym when it is overloaded.
    for s in d["left"]["symbols"]:
        if s["name"].startswith(func + "__"):
            return float(s.get("match_percent") or 0.0)
    return -1.0


ATOM = (r"(?:\(s8\)|\(u8\)|\(s32\)|\(u32\))?\*?[A-Za-z_]\w*(?:\+\+)?(?:\[[^\[\]]+\])*"
        r"(?:(?:->|\.)\w+(?:\[[^\[\]]+\])*)*|0x[0-9A-Fa-f]+|\d+")
COMM = re.compile(r"\((" + ATOM + r") ([+*|&^]) (" + ATOM + r")\)")
RHS = re.compile(r"^(\s*\w+(?:\[[^\]]+\])? = )(" + ATOM + r") ([+*|&^]) (" + ATOM + r");$")
STMT = re.compile(r"^(\s+)(\w+)(?:\[[^\]]+\])? ([-+*/|&^]?=) ([^;{}]*);$")
DECL = re.compile(r"^    (?:const )?[A-Za-z_]\w*(?:\s+|\s*\*+\s*)[A-Za-z_]\w*(?:\[[^\]]*\])*( = [^;{}]*)?;\s*$")
SETUP = re.compile(r"^    ([A-Za-z_]\w*) = ([^;{}]*);\s*$")


def ids(e):
    return set(re.findall(r"(?<![.>&\w])([A-Za-z_]\w*)", e))


def incs(e):
    return set(re.findall(r"(\w+)\+\+", e))


def has_call(e):
    return re.search(r"\w\(", e) is not None


def independent(l1, l2):
    m1, m2 = STMT.match(l1), STMT.match(l2)
    if not m1 or not m2 or m1.group(1) != m2.group(1):
        return False
    if has_call(m1.group(4)) or has_call(m2.group(4)):
        return False
    lhs1, lhs2 = l1.split("=")[0], l2.split("=")[0]
    if any(c in lhs1 for c in "[*") or any(c in lhs2 for c in "[*"):
        return False
    w1, w2 = m1.group(2), m2.group(2)
    r1, r2 = ids(m1.group(4)), ids(m2.group(4))
    if m1.group(3) != "=":
        r1.add(w1)
    if m2.group(3) != "=":
        r2.add(w2)
    if w1 == w2 or w1 in r2 or w2 in r1:
        return False
    i1, i2 = incs(l1), incs(l2)
    if i1 & (r2 | i2) or i2 & r1:
        return False
    if ("->" in l1 or "[" in l1) and "->" in lhs2:
        return False
    if ("->" in l2 or "[" in l2) and "->" in lhs1:
        return False
    return True


def header(lines):
    """Indices of the leading declaration lines and of the setup statements after them."""
    k = 1
    decls = []
    while k < len(lines) and (DECL.match(lines[k]) or not lines[k].strip()):
        if lines[k].strip():
            decls.append(k)
        k += 1
    setup = []
    while k < len(lines) and (SETUP.match(lines[k]) or not lines[k].strip()):
        if lines[k].strip():
            setup.append(k)
        k += 1
    return decls, setup


def setup_ok(order):
    """order: list of setup statement texts; every statement must stay after its dependencies."""
    seen_def = set()
    allv = {SETUP.match(l).group(1) for l in order}
    written_later = set(allv)
    for l in order:
        m = SETUP.match(l)
        lhs, rhs = m.group(1), m.group(2)
        written_later.discard(lhs)
        for u in ids(rhs):
            if u in allv and u not in seen_def and u != lhs:
                return False
        seen_def.add(lhs)
    return True


def mutate(t):
    lines = t.split("\n")
    kind = random.choice(["decl", "decl", "setup", "comm", "comm", "move", "move", "opeq"])
    decls, setup = header(lines)
    if kind == "decl":
        d = [k for k in decls if "=" not in lines[k]]
        if len(d) < 2:
            return None
        i, j = random.sample(d, 2)
        lines[i], lines[j] = lines[j], lines[i]
    elif kind == "setup":
        if len(setup) < 2:
            return None
        i, j = random.sample(setup, 2)
        texts = [lines[k] for k in setup]
        a, b = setup.index(i), setup.index(j)
        texts.insert(b, texts.pop(a))
        if not setup_ok(texts):
            return None
        for k, l in zip(setup, texts):
            lines[k] = l
    elif kind == "comm":
        cands = []
        for k, l in enumerate(lines):
            for m in COMM.finditer(l):
                cands.append((k, m.start(), m.end(), "(%s %s %s)" % (m.group(3), m.group(2), m.group(1))))
            m = RHS.match(l)
            if m:
                cands.append((k, 0, len(l), "%s%s %s %s;" % (m.group(1), m.group(4), m.group(3), m.group(2))))
        cands = [c for c in cands if "++" not in lines[c[0]][c[1]:c[2]]]
        if not cands:
            return None
        k, s, e, r = random.choice(cands)
        lines[k] = lines[k][:s] + r + lines[k][e:]
    elif kind == "move":
        cands = [k for k in range(1, len(lines) - 1)
                 if k not in decls and STMT.match(lines[k]) and STMT.match(lines[k + 1])
                 and independent(lines[k], lines[k + 1])]
        if not cands:
            return None
        k = random.choice(cands)
        lines[k], lines[k + 1] = lines[k + 1], lines[k]
    else:
        cands = []
        for k, l in enumerate(lines):
            m = re.match(r"^(\s+)(\w+) = (\w+) ([-+*|&^]|<<|>>) (" + ATOM + r");$", l)
            if m and m.group(2) == m.group(3):
                cands.append((k, "%s%s %s= %s;" % (m.group(1), m.group(2), m.group(4), m.group(5))))
            m = re.match(r"^(\s+)(\w+) ([-+*|&^]|<<|>>)= (" + ATOM + r");$", l)
            if m:
                cands.append((k, "%s%s = %s %s %s;" % (m.group(1), m.group(2), m.group(2), m.group(3), m.group(4))))
        if not cands:
            return None
        k, r = random.choice(cands)
        lines[k] = r
    return "\n".join(lines)


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Random search over equivalent source edits of one function.")
    ap.add_argument("src")
    ap.add_argument("func")
    ap.add_argument("iterations", nargs="?", type=int, default=300)
    ap.add_argument("--restarts", type=int, default=0)
    ap.add_argument("--symbol", help="symbol name to score (mangled C++ name); default: func")
    a = ap.parse_args()
    src, func, iters, restarts = a.src, a.func, a.iterations, a.restarts
    sym = a.symbol or func
    s0 = open(src).read()
    a0, b0 = span(s0, func)

    def run(t):
        open(src, "w").write(s0[:a0] + t + s0[b0:])
        return score(src, sym)

    best = s0[a0:b0]
    base = bs = run(best)
    print("base", base, flush=True)
    try:
        lines = best.split("\n")
        decls, _ = header(lines)
        plain = [k for k in decls if "=" not in lines[k]]
        for _ in range(restarts):
            c = lines[:]
            texts = [c[k] for k in plain]
            random.shuffle(texts)
            for k, l in zip(plain, texts):
                c[k] = l
            sc = run("\n".join(c))
            if sc > bs:
                bs, best = sc, "\n".join(c)
                print("restart", bs, flush=True)
        for it in range(iters):
            c = best
            for _ in range(random.choice([1, 1, 1, 2, 3])):
                m = mutate(c)
                if m:
                    c = m
            if c == best:
                continue
            sc = run(c)
            if sc > bs:
                bs, best = sc, c
                print(it, bs, flush=True)
                if bs >= 100.0:
                    break
            elif sc == bs and random.random() < 0.2:
                best = c
    finally:
        keep = bs > base
        open(src, "w").write(s0[:a0] + (best if keep else s0[a0:b0]) + s0[b0:])
        score(src, sym)
        print("best", bs if keep else base)


if __name__ == "__main__":
    main()
