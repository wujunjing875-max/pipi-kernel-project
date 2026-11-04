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
struct range { uint64_t lo, hi; };
struct level_hit { uint64_t level, array_struct, actors; unsigned actors_off; int count; };

static int fd=-1;
static uint32_t pid;
static uint64_t lib_lo, lib_hi;
static struct range ranges[1024];
static unsigned range_n;
static struct level_hit levels[64];
static unsigned level_n;

static int rd(uint64_t va, void *buf, uint64_t size){
    struct phys_io p={pid,0,va,(uint64_t)(uintptr_t)buf,size,0};
    return ioctl(fd,P_READ_PHYS,&p);
}
static uint64_t clean(uint64_t v){ return v&0x00ffffffffffffffULL; }
static int ptr(uint64_t v){
    v=clean(v); return v>=0x1000000000ULL && v<0x8000000000ULL && !(v&7);
}
static int inlib(uint64_t v){ v=clean(v); return v>=lib_lo && v<lib_hi; }
static int vt_object(uint64_t p){
    uint64_t vt=0; p=clean(p);
    return ptr(p) && rd(p,&vt,8)==0 && inlib(vt);
}
static int uobject_like(uint64_t p){
    uint64_t cls=0;
    p=clean(p);
    if(!vt_object(p)) return 0;
    for(unsigned off=0x8;off<=0x30;off+=8){
        if(rd(p+off,&cls,8)==0 && vt_object(cls)) return 1;
    }
    return 0;
}
static int actor_score(uint64_t arr, int count, int *ptrs_out, int *vts_out){
    uint64_t a[64]={0};
    int n=count<64?count:64, ptrs=0, objects=0;
    arr=clean(arr);
    if(!ptr(arr) || rd(arr,a,(uint64_t)n*8)!=0) return 0;
    for(int i=0;i<n;i++){
        uint64_t p=clean(a[i]);
        if(!ptr(p)) continue;
        ptrs++;
        if(uobject_like(p)) objects++;
    }
    *ptrs_out=ptrs; *vts_out=objects;
    return ptrs*2>=n && objects>=4 && objects*3>=ptrs*2;
}
static void save_level(uint64_t level, uint64_t ta, uint64_t arr, unsigned off, int count){
    for(unsigned i=0;i<level_n;i++) if(levels[i].level==level && levels[i].actors_off==off) return;
    if(level_n<64) levels[level_n++]=(struct level_hit){level,ta,arr,off,count};
}
static int wanted_map(const char *line, int include_blank){
    if(strstr(line,"[anon:libc_malloc]") || strstr(line,"[heap]")) return 1;
    if(!include_blank) return 0;
    const char *p=strchr(line,'/');
    if(p || strchr(line,'[')) return 0;
    return 1;
}
static void collect_maps(FILE *m, int include_blank){
    char line[1024];
    rewind(m); range_n=0;
    while(fgets(line,sizeof line,m)){
        unsigned long long lo=0,hi=0,off=0; char perm[8]={0}; int used=0;
        if(sscanf(line,"%llx-%llx %7s %llx %*s %*s %n",&lo,&hi,perm,&off,&used)<4) continue;
        if(strncmp(perm,"rw-p",4) || lo<0x1000000000ULL || !wanted_map(line+used,include_blank)) continue;
        if(hi<=lo || hi-lo>0x10000000ULL) continue;
        if(range_n<1024) ranges[range_n++]=(struct range){lo,hi};
    }
}
static void scan_arrays(void){
    uint8_t page[0x1000]; unsigned pages=0, candidates=0;
    for(unsigned r=0;r<range_n;r++){
        fprintf(stderr,"heap 0x%llx-0x%llx\n",(unsigned long long)ranges[r].lo,(unsigned long long)ranges[r].hi);
        for(uint64_t at=ranges[r].lo;at<ranges[r].hi;at+=sizeof page){
            uint64_t n=ranges[r].hi-at; if(n>sizeof page)n=sizeof page;
            if(rd(at,page,n)!=0) continue; pages++;
            for(uint64_t i=0;i+16<=n;i+=8){
                uint64_t arr=clean(*(uint64_t *)(page+i));
                int cnt=*(int *)(page+i+8), cap=*(int *)(page+i+12);
                if(!ptr(arr) || cnt<5 || cnt>6000 || cap<cnt || cap>cnt*2+64) continue;
                int ps=0,vs=0;
                if(!actor_score(arr,cnt,&ps,&vs)) continue;
                candidates++;
                uint64_t ta=at+i;
                fprintf(stdout,"array struct=0x%llx actors=0x%llx count=%d cap=%d sample_ptrs=%d sample_vt=%d\n",
                    (unsigned long long)ta,(unsigned long long)arr,cnt,cap,ps,vs);
                for(unsigned off=0x20;off<=0x500;off+=8){
                    if(ta<off) break;
                    uint64_t level=ta-off;
                    if(uobject_like(level)){
                        fprintf(stdout," level=0x%llx actors_off=0x%x\n",(unsigned long long)level,off);
                        save_level(level,ta,arr,off,cnt);
                    }
                }
                fflush(stdout);
            }
        }
    }
    fprintf(stderr,"array_scan pages=%u candidates=%u levels=%u\n",pages,candidates,level_n);
}
static void scan_level_refs(void){
    uint8_t page[0x1000]; unsigned refs=0;
    for(unsigned r=0;r<range_n;r++){
        for(uint64_t at=ranges[r].lo;at<ranges[r].hi;at+=sizeof page){
            uint64_t n=ranges[r].hi-at; if(n>sizeof page)n=sizeof page;
            if(rd(at,page,n)!=0) continue;
            for(uint64_t i=0;i+8<=n;i+=8){
                uint64_t q=clean(*(uint64_t *)(page+i));
                for(unsigned li=0;li<level_n;li++){
                    if(q!=levels[li].level) continue;
                    uint64_t field=at+i;
                    for(unsigned off=0x20;off<=0x300;off+=8){
                        if(field<off) break;
                        uint64_t world=field-off;
                        if(uobject_like(world)){
                            fprintf(stdout,"world_candidate world=0x%llx level_off=0x%x level=0x%llx actors_off=0x%x count=%d ref=0x%llx\n",
                                (unsigned long long)world,off,(unsigned long long)q,levels[li].actors_off,
                                levels[li].count,(unsigned long long)field);
                            refs++;
                        }
                    }
                }
            }
        }
    }
    fprintf(stderr,"world_refs=%u\n",refs);
}
int main(int argc,char **argv){
    if(argc<2){fprintf(stderr,"usage: scan_actor_arrays PID [blank]\n");return 2;}
    pid=(uint32_t)strtoul(argv[1],0,10);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);
    if(fd<0){fprintf(stderr,"driver fd=%d\n",fd);return 3;}
    char mp[64],line[1024]; snprintf(mp,sizeof mp,"/proc/%u/maps",pid);
    FILE *m=fopen(mp,"r"); if(!m){perror("maps");return 4;}
    while(fgets(line,sizeof line,m)){
        unsigned long long lo=0,hi=0,off=0; char perm[8]={0};
        if(!strstr(line,"libUE4.so"))continue;
        if(sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,perm,&off)!=4)continue;
        if(off==0&&!lib_lo)lib_lo=lo; if(hi>lib_hi)lib_hi=hi;
    }
    collect_maps(m,argc>2);
    fprintf(stderr,"pid=%u lib=0x%llx-0x%llx ranges=%u mode=%s\n",pid,
        (unsigned long long)lib_lo,(unsigned long long)lib_hi,range_n,argc>2?"native+blank":"libc");
    scan_arrays();
    if(level_n) scan_level_refs();
    fclose(m);
    return level_n?0:1;
}
