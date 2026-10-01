#!/usr/bin/env python3
# variants SRC UNIT FUNC VARIANTS.py : VARIANTS.py defines OLD (str) and NEWS (list of str). Tries each, prints match %.
import sys, subprocess, json, os, tempfile, runpy
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
src, unit, func, vf = sys.argv[1:5]
if not unit.startswith("main/"): unit="main/"+unit
v=runpy.run_path(vf); OLD=v["OLD"]; NEWS=v["NEWS"]
orig=open(src).read()
assert OLD in orig, "OLD not found"
obj="build/HAGE/src/"+src[len("src/"):].rsplit(".",1)[0]+".o"
def score():
    r=subprocess.run(["ninja",obj],capture_output=True,text=True)
    if r.returncode: return "compile error: "+(r.stdout+r.stderr)[-300:].replace("\n"," ")
    out=tempfile.mktemp(suffix=".json")
    subprocess.run(["build/tools/objdiff-cli","diff","-p",".","-u",unit,"-o",out,"--format","json"],capture_output=True)
    d=json.load(open(out)); os.unlink(out)
    for s in d["left"]["symbols"]:
        if s["name"]==func: return s.get("match_percent")
    return None
try:
    for i,n in enumerate(NEWS):
        open(src,"w").write(orig.replace(OLD,n))
        print(i, score(), flush=True)
finally:
    open(src,"w").write(orig)
    subprocess.run(["ninja",obj],capture_output=True)
