#!/usr/bin/env python3
"""
Generate src/pc/sdk/stubs_generated.cpp: a stub for every symbol the PC build
references but nothing defines yet.

By default the script links `newschannel_nostubs` (the program without the
stubs file), collects the linker's "undefined reference" messages and rewrites
the stubs file from scratch, so stubs for functions that have since been
implemented disappear. Stubs are weak, so a real implementation also wins
before the file is regenerated.

Usage:
    pc/tools/gen_stubs.py              relink, regenerate, print a summary
    pc/tools/gen_stubs.py --check      do not write; exit 1 if the file is stale
    pc/tools/gen_stubs.py --from FILE  take symbol names (one per line, mangled)
                                       or raw linker output from FILE ("-" = stdin)

Each stub prints "unimplemented: NAME" once and returns 0. See include/pc/stub.h
for the limits (struct-returning functions), and docs/pc_port.md, "Stubs".
"""

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
BUILD = REPO / "build" / "pc"
OUTPUT = REPO / "src" / "pc" / "sdk" / "stubs_generated.cpp"
SYMBOLS_TXT = REPO / "config" / "HAGE" / "symbols.txt"

UNDEF_RE = re.compile(r"undefined reference to `([^']+)'")
FLOAT_TYPES = r"(?:f32|f64|float|double)"


def link_undefined() -> list:
    """Link newschannel_nostubs and return the undefined symbols."""
    subprocess.run(["cmake", "-S", str(REPO / "pc"), "-B", str(BUILD), "-G", "Ninja"],
                   check=True, capture_output=True)
    result = subprocess.run(["ninja", "-C", str(BUILD), "newschannel_nostubs"],
                            capture_output=True, text=True)
    output = result.stdout + result.stderr
    symbols = sorted(set(UNDEF_RE.findall(output)))
    if result.returncode != 0 and not symbols:
        sys.exit(output[-4000:] + "\ngen_stubs: the build failed before the link step")
    return symbols


def parse_symbol_list(text: str) -> list:
    found = UNDEF_RE.findall(text)
    if found:
        return sorted(set(found))
    return sorted({line.strip() for line in text.splitlines() if line.strip()})


def demangle(symbols: list) -> dict:
    if not symbols:
        return {}
    result = subprocess.run(["c++filt"], input="\n".join(symbols), capture_output=True, text=True, check=True)
    return dict(zip(symbols, result.stdout.splitlines()))


def load_wii_symbols() -> dict:
    """name -> (type, size) from the decompilation's symbol table."""
    table = {}
    pattern = re.compile(r"^(\S+) = \S+; // type:(\w+)(?: size:0x([0-9A-Fa-f]+))?")
    for line in SYMBOLS_TXT.read_text().splitlines():
        match = pattern.match(line)
        if match:
            table[match[1]] = (match[2], int(match[3], 16) if match[3] else 0)
    return table


def load_headers() -> dict:
    """header group -> concatenated text, for include/revolution, nw4r and news."""
    groups = defaultdict(str)
    for path in sorted((REPO / "include").rglob("*.h")):
        rel = path.relative_to(REPO / "include")
        if rel.parts[0] in ("pc", "MetroTRK"):
            continue
        if rel.parts[0] == "revolution" and len(rel.parts) > 1:
            group = Path(rel.parts[1]).stem.upper()     # revolution/gx/GXVert.h -> GX
        elif rel.parts[0] == "nw4r" and len(rel.parts) > 1:
            group = "nw4r::" + Path(rel.parts[1]).stem  # nw4r/ut/... -> nw4r::ut
        elif rel.parts[0] == "news":
            group = "news"
        else:
            group = "other"
        groups[group] += path.read_text(errors="replace") + "\n"
    return groups


def classify(symbols: list):
    """Return (functions, data, problems).

    functions: [(group, symbol, text, returns_float)]
    data:      [(symbol, size)]
    problems:  [(symbol, reason)]
    """
    wii = load_wii_symbols()
    headers = load_headers()
    names = demangle(symbols)
    functions, data, problems = [], [], []

    for symbol in symbols:
        text = names.get(symbol, symbol)
        mangled = symbol.startswith("_Z")

        if mangled and text.startswith(("vtable for ", "typeinfo for ", "typeinfo name for ", "VTT for ")):
            problems.append((symbol, f"{text}: the class's first out-of-line virtual function is not "
                                     "compiled (add its file to pc/ported, or define the function)"))
            continue

        if mangled and "(" not in text:
            problems.append((symbol, f"{text}: C++ variable without a definition (port the file that defines it)"))
            continue

        if not mangled:
            kind, size = wii.get(symbol, (None, 0))
            if kind == "object":
                if size:
                    data.append((symbol, size))
                else:
                    problems.append((symbol, "variable of unknown size (define it by hand)"))
                continue

        # Short name: last component before the parameter list
        if mangled:
            head = re.sub(r"\(.*$", "", text)
            head = re.sub(r"<[^<>]*>", "", head)
            short = head.split("::")[-1].split(" ")[-1]
            namespace_match = re.match(r"(?:\w+ )*(nw4r::\w+)::", head)
            group = namespace_match[1] if namespace_match else "C++"
        else:
            short = symbol
            group = None

        decl = re.compile(r"\b" + re.escape(short) + r"\s*\(")
        float_decl = re.compile(r"\b" + FLOAT_TYPES + r"\s+(?:\w+::)*" + re.escape(short) + r"\s*\(")
        returns_float = False
        # SDK headers first: NW4R and game headers only *call* SDK functions
        for name, body in sorted(headers.items(), key=lambda kv: (not kv[0].isupper(), kv[0])):
            if not decl.search(body):
                continue
            if group is None or (not mangled and symbol.upper().startswith(name) and not symbol.upper().startswith(group)):
                group = name  # prefer the library the name is prefixed with (OSLockMutex -> OS)
            if float_decl.search(body):
                returns_float = True
        functions.append((group or "undeclared", symbol, text, returns_float))

    return functions, data, problems


def render(functions, data) -> str:
    out = [
        "// Generated by pc/tools/gen_stubs.py. Do not edit: run the script again.",
        "//",
        "// Every function here is referenced by code in the PC build but has no",
        "// implementation yet. A real definition anywhere else overrides its stub",
        "// (stubs are weak); regenerating the file then drops the stub.",
        "",
        "#include <pc/stub.h>",
        "",
    ]
    by_group = defaultdict(list)
    for group, symbol, text, returns_float in functions:
        by_group[group].append((symbol, text, returns_float))
    for group in sorted(by_group):
        entries = sorted(by_group[group], key=lambda e: e[1])
        out.append(f"// ---- {group} ({len(entries)}) ----")
        for symbol, text, returns_float in entries:
            macro = "PC_STUB_FLOAT" if returns_float else "PC_STUB"
            escaped = text.replace("\\", "\\\\").replace('"', '\\"')
            out.append(f'{macro}({symbol}, "{escaped}")')
        out.append("")
    if data:
        out.append(f"// ---- variables defined in files that are not in the build yet ({len(data)}) ----")
        out.append("// Zero-filled, sized from config/HAGE/symbols.txt.")
        for symbol, size in sorted(data):
            out.append(f"PC_STUB_DATA({symbol}, 0x{size:X})")
        out.append("")
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--from", dest="source", help="symbol list or linker output ('-' for stdin)")
    parser.add_argument("--check", action="store_true", help="do not write; exit 1 if the file would change")
    args = parser.parse_args()

    if args.source:
        text = sys.stdin.read() if args.source == "-" else Path(args.source).read_text()
        symbols = parse_symbol_list(text)
    else:
        symbols = link_undefined()

    functions, data, problems = classify(symbols)
    content = render(functions, data)

    counts = defaultdict(int)
    for group, *_ in functions:
        counts[group] += 1
    summary = ", ".join(f"{group} {n}" for group, n in sorted(counts.items(), key=lambda kv: -kv[1]))
    print(f"{len(functions)} function stubs ({summary or 'none'}), {len(data)} data stubs")

    if problems:
        print(f"\n{len(problems)} symbols cannot be stubbed and need a real definition:")
        for symbol, reason in problems:
            print(f"  {symbol}\n      {reason}")

    current = OUTPUT.read_text() if OUTPUT.exists() else ""
    if args.check:
        if current != content:
            sys.exit("stubs_generated.cpp is out of date: run pc/tools/gen_stubs.py")
    elif current != content:
        OUTPUT.write_text(content)
        print(f"wrote {OUTPUT.relative_to(REPO)}")
    else:
        print("stubs_generated.cpp is up to date")

    if problems:
        sys.exit(1)


if __name__ == "__main__":
    main()
