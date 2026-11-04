import mmap, struct, sys
path=sys.argv[1]
targets=[int(x,0) for x in sys.argv[2:]]
text_p=0x03969ae0
text_v=0x0396aae0
text_n=0x0ae3c118

def sx(v,b):
    s=1<<(b-1)
    return (v^s)-s

with open(path,'rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
    end=min(text_p+text_n,len(m))
    pages={t&~0xfff for t in targets}
    hits=[]
    for p in range(text_p,end-3,4):
        w=struct.unpack_from('<I',m,p)[0]
        if w & 0x9f000000 != 0x90000000:
            continue
        pc=text_v+(p-text_p)
        immlo=(w>>29)&3
        immhi=(w>>5)&0x7ffff
        page=(pc&~0xfff)+(sx((immhi<<2)|immlo,21)<<12)
        if page not in pages:
            continue
        rd=w&31
        for k in range(1,13):
            q=p+k*4
            if q+4>end: break
            x=struct.unpack_from('<I',m,q)[0]
            xpc=text_v+(q-text_p)
            # ADD (immediate), 64-bit
            if x & 0xff000000 == 0x91000000 and ((x>>5)&31)==rd:
                imm=((x>>10)&0xfff) << (12 if ((x>>22)&1) else 0)
                addr=page+imm
                if addr in targets:
                    hits.append((pc,xpc,addr,'add',rd,x&31))
            # LDR unsigned immediate, X register
            if x & 0xffc00000 == 0xf9400000 and ((x>>5)&31)==rd:
                addr=page+(((x>>10)&0xfff)*8)
                if addr in targets:
                    hits.append((pc,xpc,addr,'ldr',rd,x&31))
    for h in hits:
        print('adrp=0x%x use=0x%x target=0x%x kind=%s base=x%d out=x%d'%h)
    print('hits=%d'%len(hits))
