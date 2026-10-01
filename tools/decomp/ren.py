#!/usr/bin/env python3
# ren OLD=NEW[:local|:weak] ... : rename symbols in config/HAGE/symbols.txt (and nothing else)
import sys, re, os
os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
p="config/HAGE/symbols.txt"; lines=open(p).read().split("\n")
idx={l.split(" = ")[0]:i for i,l in enumerate(lines) if " = " in l}
names=set(idx)
for arg in sys.argv[1:]:
    old,new=arg.split("=",1); scope=None
    m=re.match(r"(.*):(local|weak|global)$",new)
    if m: new,scope=m.group(1),m.group(2)
    if old not in idx: print("missing",old); continue
    if new in names and new!=old and scope!="local": print("exists",new); continue
    i=idx[old]; l=lines[i]
    l=new+l[len(old):]
    if scope:
        l=re.sub(r" scope:\w+","",l)
        l=l.replace("// ","// ",1).rstrip()+f" scope:{scope}"
    lines[i]=l; names.add(new)
open(p,"w").write("\n".join(lines))
