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

struct phys_io { uint32_t pid, pad; uint64_t va, buf, size, pa_out; };
static int fd = -1;
static uint32_t pid = 0;
static uint64_t base = 0, lib_end = 0;
static int rd(uint64_t va, void *buf, uint64_t size) {
    struct phys_io p = { pid, 0, va, (uint64_t)(uintptr_t)buf, size, 0 };
    return ioctl(fd, P_READ_PHYS, &p);
}
static uint64_t clean(uint64_t v) { return v & 0x00ffffffffffffffULL; }
static int ptr(uint64_t v) {
    v = clean(v);
    return v >= 0x1000000000ULL && v < 0x8000000000ULL && !(v & 7);
}
static int inlib(uint64_t v) { v=clean(v); return v>=base && v<lib_end; }
static int object_like(uint64_t p) {
    uint64_t vt=0, cls=0, cvt=0;
    p=clean(p);
    if (!ptr(p) || rd(p,&vt,8)!=0 || !inlib(vt)) return 0;
    if (rd(p+0x10,&cls,8)!=0 || !ptr(cls)) return 0;
    cls=clean(cls);
    if (rd(cls,&cvt,8)!=0 || !inlib(cvt)) return 0;
    return 1;
}
static unsigned actor_score(uint64_t arr, int cnt) {
    uint64_t a[32]={0};
    int n=cnt<32?cnt:32, pointerish=0, objects=0;
    arr=clean(arr);
    if (!ptr(arr) || rd(arr,a,(uint64_t)n*8)!=0) return 0;
    for (int i=0;i<n;i++) {
        uint64_t p=clean(a[i]);
        if (!p) continue;
        if (!ptr(p)) continue;
        pointerish++;
        if (objects<8 && object_like(p)) objects++;
    }
    if (pointerish<3 || objects<2) return 0;
    return (unsigned)(pointerish*10+objects);
}

#define SEEN_N 65536u
static uint64_t seen[SEEN_N];
static int seen_add(uint64_t p) {
    uint32_t h=(uint32_t)((p>>4)^(p>>21)^(p>>37))&(SEEN_N-1);
    for (uint32_t k=0;k<32;k++) {
        uint32_t i=(h+k)&(SEEN_N-1);
        if (seen[i]==p) return 0;
        if (!seen[i]) { seen[i]=p; return 1; }
    }
    return 0;
}

static unsigned probe_world(uint64_t global, uint64_t world) {
    uint8_t wb[0x200];
    world=clean(world);
    if (!seen_add(world) || !object_like(world) || rd(world,wb,sizeof wb)!=0) return 0;
    unsigned hits=0;
    for (unsigned wo=0x20;wo<=0x1b0;wo+=8) {
        uint64_t level=clean(*(uint64_t *)(wb+wo));
        if (!ptr(level) || !object_like(level)) continue;
        uint8_t lb[0x380];
        if (rd(level,lb,sizeof lb)!=0) continue;
        for (unsigned ao=0x40;ao<=0x360;ao+=8) {
            uint64_t arr=clean(*(uint64_t *)(lb+ao));
            int cnt=*(int *)(lb+ao+8), cap=*(int *)(lb+ao+12);
            if (!ptr(arr) || cnt<3 || cnt>10000 || cap<cnt || cap>20000) continue;
            unsigned score=actor_score(arr,cnt);
            if (!score) continue;
            printf("candidate global_off=0x%llx global=0x%llx world=0x%llx level_off=0x%x level=0x%llx actors_off=0x%x arr=0x%llx cnt=%d cap=%d score=%u\n",
                (unsigned long long)(global-base),(unsigned long long)global,
                (unsigned long long)world,wo,(unsigned long long)level,ao,
                (unsigned long long)arr,cnt,cap,score);
            fflush(stdout); hits++;
        }
    }
    return hits;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: scan_gworld PID\n"); return 2; }
    pid = (uint32_t)strtoul(argv[1], 0, 10);
    syscall(__NR_reboot, (long)M1, (long)M2, 0x1234567L, &fd);
    if (fd < 0) { fprintf(stderr, "driver fd=%d\n", fd); return 3; }

    char mp[64], line[768]; snprintf(mp, sizeof mp, "/proc/%u/maps", pid);
    FILE *m = fopen(mp, "r"); if (!m) { perror("maps"); return 4; }
    while (fgets(line, sizeof line, m)) {
        unsigned long long lo=0, hi=0, off=0; char perms[8]={0};
        if (!strstr(line, "libUE4.so")) continue;
        if (sscanf(line, "%llx-%llx %7s %llx", &lo, &hi, perms, &off) != 4) continue;
        if (off == 0 && !base) base = lo;
        if (hi > lib_end) lib_end = hi;
    }
    rewind(m);
    fprintf(stderr, "base=0x%llx end=0x%llx fd=%d\n",
        (unsigned long long)base,(unsigned long long)lib_end,fd);

    unsigned hits=0, objects=0, vtglobals=0, chunks=0;
    uint8_t *chunk=malloc(0x1000); if(!chunk) return 5;
    while (fgets(line, sizeof line, m)) {
        unsigned long long lo=0, hi=0, off=0; char perms[8]={0};
        if (!strstr(line, "libUE4.so")) continue;
        if (sscanf(line, "%llx-%llx %7s %llx", &lo, &hi, perms, &off) != 4) continue;
        if (strncmp(perms,"rw-p",4) || off<0xe000000ULL) continue;
        fprintf(stderr,"scan 0x%llx-0x%llx off=0x%llx\n",lo,hi,off);
        for(uint64_t at=lo;at<hi;at+=0x1000) {
            uint64_t n=hi-at; if(n>0x1000)n=0x1000;
            if(rd(at,chunk,n)!=0) continue; chunks++;
            for(uint64_t i=0;i+8<=n;i+=8) {
                uint64_t world=clean(*(uint64_t *)(chunk+i));
                if(!ptr(world) || inlib(world)) continue;
                uint64_t vt0=0;
                if(rd(world,&vt0,8)==0 && inlib(vt0)){
                    if(vtglobals<20){
                        fprintf(stderr," vtglobal(off=0x%llx ptr=0x%llx vt=0x%llx)",
                            (unsigned long long)((at+i)-base),(unsigned long long)world,
                            (unsigned long long)clean(vt0));
                        uint64_t dump[64]={0};
                        if(rd(world,dump,sizeof dump)==0){
                            for(unsigned di=0;di<64;di++){
                                if((di&3)==0) fprintf(stderr," object+0x%03x:",di*8);
                                fprintf(stderr," %016llx",(unsigned long long)dump[di]);
                                if((di&3)==3) fputc(10,stderr);
                            }
                        }
                    }
                    vtglobals++;
                }
                if(object_like(world)) objects++;
                hits += probe_world(at+i,world);
            }
        }
    }
    fclose(m); free(chunk);
    fprintf(stderr,"chunks=%u vt_globals=%u object_globals=%u hits=%u\n",chunks,vtglobals,objects,hits);
    return hits?0:1;
}
