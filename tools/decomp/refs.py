#!/usr/bin/env python3
# refs START END : list data symbols referenced by .text in [START,END), with section/addr and outside users
import sys, glob, re, os, collections
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
S=int(sys.argv[1],16); E=int(sys.argv[2],16)
syms={}
for l in open("config/HAGE/symbols.txt"):
    m=re.match(r"(\S+) = (\S+):0x([0-9A-Fa-f]+); // (.*)",l)
    if m: syms[m.group(1)]=(m.group(2),int(m.group(3),16),m.group(4))
pat=re.compile(r"^/\* ([0-9A-F]{8}) [0-9A-F]{8}  [0-9A-F ]{11} \*/\t(.*)$")
users=collections.defaultdict(set)
cur=None
for f in glob.glob("build/HAGE/asm/**/*.s",recursive=True):
    sec=None; curobj=None
    for l in open(f):
        if l.startswith(".section") or l.startswith(".text"): sec=l.split()[1].rstrip(",") if l.startswith(".section") else ".text"
        m=re.match(r"^\.(fn|obj) (\S+),",l)
        if m: curobj=m.group(2)
        mm=pat.match(l)
        if mm:
            a=int(mm.group(1),16)
            for s in re.findall(r"([A-Za-z_@$][\w$.]*)(?:@ha|@l|@sda21|\b)",mm.group(2)):
                if s in syms: users[s].add(a)
        elif l.startswith("\t.4byte"):
            s=l.split()[1]
            if s in syms and curobj in syms: users[s].add(("data",curobj))
for s,u in sorted(users.items(), key=lambda kv: (syms[kv[0]][0], syms[kv[0]][1])):
    inside=[x for x in u if isinstance(x,int) and S<=x<E]
    if not inside: continue
    outside=sorted(set(hex(x) if isinstance(x,int) else x[1] for x in u if not (isinstance(x,int) and S<=x<E)))
    sec,addr,info=syms[s]
    if sec==".text" : 
        if S<=addr<E: continue
        print(f"CALL {s:40} {hex(addr)}"); continue
    sz=re.search(r"size:(0x[0-9A-F]+)",info); sz=sz.group(1) if sz else "?"
    print(f"{sec:8} {hex(addr)} {sz:6} {s:36} outside={outside[:6]}{'...' if len(outside)>6 else ''}")
