#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_READ_PHYS 0xC0285709u
#define NAME_POOL_OFF 0x10628080ULL
#define NAME_BLOCKS_OFF 0x48ULL

struct phys_io { uint32_t pid, pad; uint64_t va, buf, size, pa_out; };
static int fd = -1;
static uint32_t pid;
static uint64_t base, end, names;

static int rd(uint64_t a, void *b, uint64_t n) {
    struct phys_io p = { pid, 0, a, (uint64_t)(uintptr_t)b, n, 0 };
    return ioctl(fd, P_READ_PHYS, &p);
}
static uint64_t cl(uint64_t v) { return v & 0x00ffffffffffffffULL; }
static int ptr(uint64_t v) { v=cl(v); return v>=0x1000000000ULL && v<0x8000000000ULL && !(v&7); }
static int inlib(uint64_t v) { v=cl(v); return v>=base && v<end; }

static int fname(uint32_t idx, char *out, int cap) {
    out[0]=0;
    uint32_t block = idx >> 16;
    uint32_t off = (idx & 0xffffu) * 2u;
    uint64_t b=0;
    if (block >= 8192 || rd(names + NAME_BLOCKS_OFF + (uint64_t)block*8, &b, 8)) return 0;
    b=cl(b); if (!ptr(b)) return 0;
    uint16_t h=0; if (rd(b+off,&h,2)) return 0;
    int len=(h>>6)&0x3ff; int wide=h&1;
    if (len<1 || len>=cap || wide) return 0;
    if (rd(b+off+2,out,(uint64_t)len)) return 0;
    out[len]=0;
    for(int i=0;i<len;i++) if((unsigned char)out[i]<0x20 || (unsigned char)out[i]>0x7e) return 0;
    return 1;
}

static int obj_name(uint64_t obj, char *out, int cap, uint64_t *cls_out, uint32_t *idx_out) {
    uint64_t cls=0; uint32_t idx=0;
    obj=cl(obj); if(!ptr(obj) || rd(obj+0x10,&cls,8)) return 0;
    cls=cl(cls); if(!ptr(cls) || rd(cls+0x18,&idx,4)) return 0;
    if(cls_out)*cls_out=cls; if(idx_out)*idx_out=idx;
    return fname(idx,out,cap);
}

int main(int argc,char **argv) {
    if(argc<3){fprintf(stderr,"usage: resolve_array_names PID TARRAY_STRUCT [limit]\n");return 2;}
    pid=(uint32_t)strtoul(argv[1],0,10); uint64_t ta=strtoull(argv[2],0,0);
    int limit=argc>3?atoi(argv[3]):128; if(limit<1)limit=1; if(limit>2048)limit=2048;
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd); if(fd<0){perror("driver");return 3;}
    char mp[64],line[1024]; snprintf(mp,sizeof mp,"/proc/%u/maps",pid); FILE *m=fopen(mp,"r"); if(!m)return 4;
    while(fgets(line,sizeof line,m)){unsigned long long lo,hi,off;char pm[8];
        if(strstr(line,"libUE4.so")&&sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)==4){if(off==0&&!base)base=lo;if(hi>end)end=hi;}}
    fclose(m); names=base+NAME_POOL_OFF;
    uint8_t head[0x90]={0}; rd(names,head,sizeof head);
    fprintf(stderr,"pid=%u lib=0x%llx-0x%llx names=0x%llx block0=0x%llx\n",pid,
        (unsigned long long)base,(unsigned long long)end,(unsigned long long)names,
        (unsigned long long)cl(*(uint64_t*)(head+NAME_BLOCKS_OFF)));
    struct {uint64_t p;int n,cap;} tv={0}; if(rd(ta,&tv,sizeof tv)){fprintf(stderr,"read tarray failed\n");return 5;}
    tv.p=cl(tv.p); printf("tarray=0x%llx data=0x%llx count=%d cap=%d\n",(unsigned long long)ta,(unsigned long long)tv.p,tv.n,tv.cap);
    for(unsigned back=0x20;back<=0x500;back+=8){uint64_t o=ta-back,cls=0;uint32_t idx=0;char nm[160];
        if(obj_name(o,nm,sizeof nm,&cls,&idx))printf("host off=0x%x obj=0x%llx class=0x%llx idx=%u name=%s\n",back,(unsigned long long)o,(unsigned long long)cls,idx,nm);}
    if(tv.n<limit)limit=tv.n; uint64_t *a=calloc((size_t)limit,8); if(!a)return 6; if(rd(tv.p,a,(uint64_t)limit*8))return 7;
    int ok=0;
    for(int i=0;i<limit;i++){uint64_t cls=0;uint32_t idx=0;char nm[160];
        if(obj_name(a[i],nm,sizeof nm,&cls,&idx)){printf("[%d] obj=0x%llx class=0x%llx idx=%u name=%s%s\n",i,(unsigned long long)cl(a[i]),(unsigned long long)cls,idx,nm,inlib(a[i])?" INLIB":"");ok++;}}
    fprintf(stderr,"resolved=%d/%d\n",ok,limit); free(a); return ok?0:1;
}
