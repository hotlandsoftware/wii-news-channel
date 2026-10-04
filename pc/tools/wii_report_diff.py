#!/usr/bin/env python3
"""
Compare two build/HAGE/report.json files of the Wii build and list every unit
or function whose match percentage changed. PC-port changes to shared files
must leave the Wii build untouched, so the expected output is "no differences".

Usage:
    cp build/HAGE/report.json /tmp/before.json     # before your change
    python3 configure.py && rm -f build/HAGE/main.dol && ninja
    pc/tools/wii_report_diff.py /tmp/before.json   # compares with build/HAGE/report.json
    pc/tools/wii_report_diff.py before.json after.json
"""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent


def load(path: Path) -> dict:
    report = json.loads(path.read_text())
    out = {}
    for unit in report["units"]:
        name = unit["name"]
        out[(name, None)] = unit.get("measures", {}).get("fuzzy_match_percent")
        for function in unit.get("functions", []):
            out[(name, function["name"])] = function.get("fuzzy_match_percent", 0)
    return out


def main() -> None:
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    before = load(Path(sys.argv[1]))
    after = load(Path(sys.argv[2]) if len(sys.argv) == 3 else REPO / "build" / "HAGE" / "report.json")

    changes = []
    for key in sorted(set(before) | set(after), key=lambda k: (k[0], k[1] or "")):
        old, new = before.get(key, "missing"), after.get(key, "missing")
        if old != new:
            unit, function = key
            changes.append(f"  {unit}{' :: ' + function if function else ''}: {old} -> {new}")

    if changes:
        print(f"{len(changes)} differences:")
        print("\n".join(changes))
        sys.exit(1)
    print(f"no differences ({len(after)} units and functions compared)")


if __name__ == "__main__":
    main()
