#!/usr/bin/env python3
# dump START END : hex words with float interpretation from the DOL
import struct,sys,os
d=open(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),"orig/HAGE/sys/main.dol"),"rb").read()
off=struct.unpack(">18I",d[0:72]);addr=struct.unpack(">18I",d[72:144]);sz=struct.unpack(">18I",d[144:216])
def rd(a,n):
    for i in range(18):
        if sz[i] and addr[i]<=a<addr[i]+sz[i]: o=off[i]+a-addr[i]; return d[o:o+n]
    return b"\0"*n
a=int(sys.argv[1],16); e=int(sys.argv[2],16)
while a<e:
    b=rd(a,4); v=struct.unpack(">I",b)[0]; f=struct.unpack(">f",b)[0]
    print(f"{a:08X}  {b.hex()}  {v:10d}  {f:g}")
    a+=4
