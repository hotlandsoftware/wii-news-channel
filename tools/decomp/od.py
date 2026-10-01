#!/usr/bin/env python3
# od UNIT [SYMBOL-substring] [-a]: side-by-side diff (left=target, right=ours). -a shows all lines.
import json, sys, subprocess, os, tempfile
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
unit=sys.argv[1]
if not unit.startswith("main/"): unit="main/"+unit
sym=sys.argv[2] if len(sys.argv)>2 and not sys.argv[2].startswith("-") else None
showall="-a" in sys.argv
out=tempfile.mktemp(suffix=".json")
r=subprocess.run(["build/tools/objdiff-cli","diff","-p",".","-u",unit,"-o",out,"--format","json"],capture_output=True,text=True)
if r.returncode: print(r.stderr[-2000:]); sys.exit(1)
d=json.load(open(out)); os.unlink(out)
L=d["left"]["symbols"]; R=d["right"]["symbols"]
def fmt(row):
    if not row or "instruction" not in row: return ""
    i=row["instruction"]; s=i["formatted"]
    if "relocation" in i:
        t=i["relocation"].get("target_symbol")
        s+=f"  <{i['relocation'].get('_name','')}>"
    return s
for li,ls in enumerate(L):
    if "instructions" not in ls: continue
    if sym and sym not in ls["name"]: continue
    rs=next((x for x in R if x["name"]==ls["name"]),None)
    print(f"== {ls['name']}  {ls.get('match_percent')}")
    if not rs: print("   (no match on right)"); continue
    a=ls["instructions"]; b=rs["instructions"]
    def nm(side,row):
        if row and "instruction" in row and "relocation" in row["instruction"]:
            t=row["instruction"]["relocation"].get("target_symbol"); return side[t]["name"] if t is not None and t<len(side) else "?"
        return ""
    for k in range(max(len(a),len(b))):
        x=a[k] if k<len(a) else {}; y=b[k] if k<len(b) else {}
        dk=x.get("diff_kind") or y.get("diff_kind") or ""
        if not showall and not dk: continue
        xs=fmt(x).split("  <")[0]+(" ="+nm(L,x) if nm(L,x) else ""); ys=fmt(y).split("  <")[0]+(" ="+nm(R,y) if nm(R,y) else "")
        mark={"":" ","DIFF_ARG_MISMATCH":"r","DIFF_REPLACE":"|","DIFF_DELETE":"<","DIFF_INSERT":">","DIFF_OP_MISMATCH":"|"}.get(dk,"?")
        print(f"{k*4:4x} {xs[:52]:52} {mark} {ys[:52]}")
