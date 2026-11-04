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
struct phys_io{uint32_t pid,pad;uint64_t va,buf,size,pa_out;};
static int fd=-1;static uint32_t pid;static uint64_t base,end;
static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}
static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}
static int ptr(uint64_t v){v=cl(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}
static int inlib(uint64_t v){v=cl(v);return v>=base&&v<end;}
static int vtobj(uint64_t p){uint64_t v=0;p=cl(p);return ptr(p)&&rd(p,&v,8)==0&&inlib(v);}
static void objline(const char *tag,uint64_t p){
    uint64_t q[10]={0};p=cl(p);rd(p,q,sizeof q);
    printf("%s p=0x%llx vt=0x%llx(vtoff=0x%llx)",tag,(unsigned long long)p,
        (unsigned long long)cl(q[0]),(unsigned long long)(inlib(q[0])?cl(q[0])-base:0));
    for(unsigned o=8;o<=0x30;o+=8){
        uint64_t x=cl(q[o/8]);
        if(vtobj(x)){
            uint32_t idx=0; rd(x+0x18,&idx,4);
            printf(" cls@+0x%x=0x%llx nameidx=%u",o,(unsigned long long)x,idx);
        }
    }
    printf(" raw10=0x%llx raw18=0x%llx\n",(unsigned long long)q[2],(unsigned long long)q[3]);
}
int main(int argc,char**argv){
    if(argc!=4){fprintf(stderr,"usage: inspect_array PID ARRAY_STRUCT ACTORS\n");return 2;}
    pid=(uint32_t)strtoul(argv[1],0,10);uint64_t ta=strtoull(argv[2],0,0),arr=strtoull(argv[3],0,0);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;
    char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");
    while(fgets(line,sizeof line,m)){unsigned long long lo,hi,off;char pm[8];if(strstr(line,"libUE4.so")&&sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)==4){if(off==0&&!base)base=lo;if(hi>end)end=hi;}}
    fclose(m);fprintf(stderr,"base=0x%llx end=0x%llx\n",(unsigned long long)base,(unsigned long long)end);
    struct { uint64_t p; int n, cap; } tv={0};
    if(rd(ta,&tv,sizeof tv)==0){
        printf("tarray struct=0x%llx actors=0x%llx count=%d cap=%d\n",
            (unsigned long long)ta,(unsigned long long)cl(tv.p),tv.n,tv.cap);
        arr=cl(tv.p);
    }
    for(unsigned off=0x20;off<=0x500;off+=8){uint64_t p=ta-off;if(vtobj(p)){char tag[48];snprintf(tag,sizeof tag,"host off=0x%x",off);objline(tag,p);}}
    uint64_t a[32]={0};rd(arr,a,sizeof a);
    for(unsigned i=0;i<32;i++){if(!ptr(a[i]))continue;char tag[48];snprintf(tag,sizeof tag,"actor[%u]",i);objline(tag,a[i]);}
    return 0;
}
