#!/usr/bin/env python3
# rep.py UNIT-SUBSTRING : rebuild report.json and print per-function fuzzy match for matching units
import json, os, subprocess, sys
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
r = subprocess.run(["ninja", "build/HAGE/report.json"], capture_output=True, text=True)
out = r.stdout + r.stderr
if r.returncode:
    lines = out.splitlines()
    idx = [i for i, l in enumerate(lines) if "rror" in l]
    for i in idx[:3]:
        print("\n".join(lines[i:i + 6]))
d = json.load(open("build/HAGE/report.json"))
for u in d["units"]:
    if sys.argv[1] in u["name"]:
        print(u["name"], u.get("measures", {}).get("fuzzy_match_percent"))
        for f in u.get("functions", []):
            print(f"  {f.get('fuzzy_match_percent', 0):7.3f} {f['size']:>5} {f['name']}")
