#!/usr/bin/env python3
"""
Compile status of the PC port: which files of src/news and src/nw4r compile
with the PC flags.

Every file is compiled with -fsyntax-only, using the exact command CMake
generated for it (the `status_<library>` targets in build/pc/compile_commands.json),
so files that are not in the build yet (pc/ported/<library>.txt) are measured too.

Usage:
    pc/tools/status.py                 summary table per library
    pc/tools/status.py -v              ... plus the first error of every failing file
    pc/tools/status.py -f ut_Font      only files whose path contains the text
                                       (prints the complete compiler output)
    pc/tools/status.py --categories    group the first errors by kind
    pc/tools/status.py --update-ported add every compiling file to pc/ported/*.txt
    pc/tools/status.py --json out.json machine-readable result
"""

import argparse
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
BUILD = REPO / "build" / "pc"
PORTED = REPO / "pc" / "ported"

ERROR_RE = re.compile(r"^(?P<file>[^:\s]+):(?P<line>\d+):(?:\d+:)? (?:fatal )?error: (?P<msg>.*)$")


def configure() -> None:
    """(Re)run CMake so compile_commands.json is current."""
    cmd = ["cmake", "-S", str(REPO / "pc"), "-B", str(BUILD), "-G", "Ninja"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(result.stdout + result.stderr)


def load_commands():
    """Return [(library, source path, argv)] for the status_* targets."""
    entries = json.loads((BUILD / "compile_commands.json").read_text())
    out = []
    for entry in entries:
        match = re.search(r"CMakeFiles/status_(\w+)\.dir/", entry.get("output", entry["command"]))
        if not match:
            continue
        argv = shlex.split(entry["command"])
        # Drop `-o <obj>`, `-c`, and dependency-file options; check syntax only.
        cleaned = []
        skip = False
        for arg in argv:
            if skip:
                skip = False
                continue
            if arg in ("-o", "-MF", "-MT", "-MQ"):
                skip = True
                continue
            if arg in ("-c", "-MD", "-MMD"):
                continue
            cleaned.append(arg)
        cleaned += ["-fsyntax-only", "-fmax-errors=20", "-fdiagnostics-color=never",
                    "-fno-diagnostics-show-caret"]
        out.append((match.group(1), Path(entry["file"]), cleaned, entry["directory"]))
    return sorted(out, key=lambda e: (e[0], str(e[1])))


def compile_one(item):
    library, path, argv, directory = item
    result = subprocess.run(argv, cwd=directory, capture_output=True, text=True)
    first = None
    count = 0
    for line in result.stderr.splitlines():
        match = ERROR_RE.match(line)
        if match:
            count += 1
            if first is None:
                try:
                    where = Path(match["file"]).resolve().relative_to(REPO)
                except ValueError:
                    where = match["file"]
                first = f"{where}:{match['line']}: {match['msg']}"
    ok = result.returncode == 0
    if not ok and first is None:
        first = (result.stderr.strip().splitlines() or ["(no output)"])[-1]
    return library, path, ok, first, count, result.stderr


def categorize(message: str) -> str:
    """Rough kind of a first error, for the --categories table."""
    msg = message.split(": ", 1)[1] if ": " in message else message
    rules = [
        (r"expected .\(. before|expected string-literal or constexpr in parentheses|\basm\b",
         "inline asm / asm function (needs a C version under TARGET_PC)"),
        (r"jump to case label|crosses initialization", "jump over an initialisation (switch case needs braces)"),
        (r"No such file or directory", "missing header"),
        (r"lvalue required", "cast used as lvalue"),
        (r"cannot be overloaded|redeclared|redefinition|conflicting declaration|ambiguating|conflicts with",
         "conflicting declaration (often u32 vs unsigned int)"),
        (r"cannot convert|invalid conversion|no matching function|could not convert|invalid cast|"
         r"invalid .static_cast.|invalid initialization", "type conversion CodeWarrior accepts"),
        (r"not usable in a constant expression|is not a constant expression", "constant expression"),
        (r"was not declared in this scope|has not been declared|not a member of|does not name a type",
         "undeclared name"),
        (r"changes meaning", "name changes meaning"),
        (r"template|dependent|typename", "template / dependent name"),
        (r"expected", "syntax"),
    ]
    for pattern, name in rules:
        if re.search(pattern, msg):
            return name
    return "other"


def read_ported(library: str):
    path = PORTED / f"{library}.txt"
    if not path.exists():
        return [], []
    lines = path.read_text().splitlines()
    names = [l.strip() for l in lines if l.strip() and not l.strip().startswith("#")]
    return lines, names


def update_ported(results, source_dirs) -> None:
    by_lib = defaultdict(list)
    for library, path, ok, *_ in results:
        if ok:
            by_lib[library].append(path)
    for library, paths in sorted(by_lib.items()):
        lines, names = read_ported(library)
        if "*" in names:
            continue
        base = source_dirs[library]
        new = sorted(set(names) | {str(p.relative_to(base)) for p in paths})
        if new == sorted(names):
            continue
        header = [l for l in lines if l.strip().startswith("#")] or [
            "# Files of this library that are built into newschannel (see docs/pc_port.md)."
        ]
        (PORTED / f"{library}.txt").write_text("\n".join(header + new) + "\n")
        print(f"pc/ported/{library}.txt: {len(names)} -> {len(new)} files")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-v", "--verbose", action="store_true", help="list failing files with their first error")
    parser.add_argument("-f", "--filter", help="only files whose path contains this text; print full output")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--categories", action="store_true", help="group first errors by kind")
    parser.add_argument("--update-ported", action="store_true", help="add compiling files to pc/ported/*.txt")
    parser.add_argument("--json", type=Path, help="write the result as JSON")
    parser.add_argument("--no-configure", action="store_true", help="do not rerun CMake first")
    args = parser.parse_args()

    if not args.no_configure:
        configure()
    items = load_commands()
    if args.filter:
        items = [i for i in items if args.filter in str(i[1])]
    if not items:
        sys.exit("no files to check")

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(compile_one, items))

    if args.filter:
        for library, path, ok, first, count, stderr in results:
            print(f"== {path.relative_to(REPO)}: {'ok' if ok else 'FAILED'}")
            if not ok:
                print(stderr.rstrip())

    # Library source directories, from the news_library() calls in CMakeLists.txt
    source_dirs = {
        name: REPO / directory
        for name, directory in re.findall(
            r"news_library\((\w+)\s+DIR\s+(\S+)\)", (REPO / "pc" / "CMakeLists.txt").read_text())
    }

    totals = defaultdict(lambda: [0, 0])
    for library, path, ok, *_ in results:
        totals[library][1] += 1
        totals[library][0] += ok

    print(f"{'library':<12} {'compiles':>8} {'total':>6} {'%':>7}   in build")
    all_ok = all_n = 0
    for library, (n_ok, n) in sorted(totals.items()):
        _, names = read_ported(library)
        in_build = "all" if "*" in names else str(len(names))
        print(f"{library:<12} {n_ok:>8} {n:>6} {100.0 * n_ok / n:>6.1f}%   {in_build}")
        all_ok += n_ok
        all_n += n
    print(f"{'TOTAL':<12} {all_ok:>8} {all_n:>6} {100.0 * all_ok / all_n:>6.1f}%")

    failing = [r for r in results if not r[2]]
    if args.verbose and failing:
        print("\nFailing files (first error):")
        for library, path, ok, first, count, _ in failing:
            print(f"  {path.relative_to(REPO)} ({count} errors)\n      {first}")

    if args.categories and failing:
        kinds = Counter(categorize(r[3]) for r in failing)
        print("\nFirst errors by kind:")
        for kind, n in kinds.most_common():
            print(f"  {n:>4}  {kind}")
        where = Counter(r[3].split(":", 1)[0] for r in failing)
        print("\nFirst errors by file where they occur (shared headers first):")
        for name, n in where.most_common(15):
            print(f"  {n:>4}  {name}")

    if args.json:
        args.json.write_text(json.dumps([
            {"library": lib, "file": str(path.relative_to(REPO)), "ok": ok,
             "first_error": first, "errors": count}
            for lib, path, ok, first, count, _ in results
        ], indent=1))

    if args.update_ported:
        update_ported(results, source_dirs)


if __name__ == "__main__":
    main()
