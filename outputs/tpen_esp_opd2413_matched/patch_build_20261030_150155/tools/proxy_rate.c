// proxy_rate.c —— 量化代理体坐标更新频率，并寻找更新更快的坐标字段
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <time.h>

#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_FIND_PROC 0xC104570Bu
#define P_MODULE_BASE 0xC120570Au
#define P_READ_PHYS 0xC0285709u
#define G_WORLD  0xfbeee10ULL
#define GOFF_GNAMES 0xf987338ULL

struct find_io{uint32_t pid;char name[0x100];};
struct mod_io{uint32_t pid;char name[0x100];uint64_t base;uint32_t size;uint32_t pad;};
struct phys_io{uint32_t pid;uint32_t pad;uint64_t va;uint64_t buf;uint64_t size;uint64_t pa_out;};
static int FD=-1; static uint32_t PID=0; static uint64_t GN=0;
static int rd(uint64_t va,void* b,uint64_t n){
    struct phys_io p; memset(&p,0,sizeof p);
    p.pid=PID; p.va=va; p.buf=(uint64_t)(uintptr_t)b; p.size=n;
    return ioctl(FD,P_READ_PHYS,&p);
}
static uint64_t r64(uint64_t va){uint64_t v=0;return rd(va,&v,8)==0?(v&0x00ffffffffffffffULL):0;}
static uint32_t r32(uint64_t va){uint32_t v=0;return rd(va,&v,4)==0?v:0;}
static int isp(uint64_t v){return v>0x10000000ULL&&v<0x7fffffffffffULL&&(v&7)==0;}
static int fname(uint32_t idx,char*out,int cap){
    out[0]=0; if(!idx) return 0;
    uint32_t blkidx=idx>>16, off=(idx&0xffff)*2;
    uint64_t blk=0;
    if(rd(GN+0x10+(uint64_t)blkidx*8,&blk,8)!=0) return 0;
    blk&=0x00ffffffffffffffULL; if(blk<0x10000ULL) return 0;
    uint16_t h=0; if(rd(blk+off,&h,2)!=0) return 0;
    int len=(h>>6)&0x3ff; if(len<1||len>=cap) return 0;
    if(rd(blk+off+2,out,len)!=0) return 0;
    out[len]=0;
    for(int i=0;i<len;i++){unsigned char c=out[i]; if(c<0x20||c>0x7e) return 0;}
    return 1;
}
static int cls_name(uint64_t actor,char*out,int cap){
    out[0]=0; uint64_t cls=0;
    if(rd(actor+0x10,&cls,8)!=0) return 0;
    cls&=0x00ffffffffffffffULL; if(cls<0x10000ULL) return 0;
    uint32_t idx=0; if(rd(cls+0x18,&idx,4)!=0) return 0;
    return fname(idx,out,cap);
}

#define WIN 0x600
int main(void){
    register long x0 __asm__("x0")=(long)M1;
    register long x1 __asm__("x1")=(long)M2;
    register long x2 __asm__("x2")=(long)0x1234567;
    register long x3 __asm__("x3")=(long)&FD;
    register long x8 __asm__("x8")=__NR_reboot;
    __asm__ __volatile__("svc #0":"+r"(x0):"r"(x0),"r"(x1),"r"(x2),"r"(x3),"r"(x8):"memory","cc");
    if(FD<0)return 1;
    struct find_io fi; memset(&fi,0,sizeof fi);
    snprintf(fi.name,sizeof fi.name,"com.tencent.mf.uam");
    if(ioctl(FD,P_FIND_PROC,&fi)!=0||!fi.pid)return 2;
    PID=fi.pid;
    struct mod_io mo; memset(&mo,0,sizeof mo); mo.pid=PID;
    snprintf(mo.name,sizeof mo.name,"libUE4.so");
    if(ioctl(FD,P_MODULE_BASE,&mo)!=0||!mo.base)return 3;
    uint64_t base=mo.base; GN=base+GOFF_GNAMES;

    uint64_t world=r64(base+G_WORLD);
    uint64_t level=world?r64(world+0x30):0;
    uint64_t actors=level?r64(level+0x98):0;
    uint32_t cnt=level?r32(level+0xa0):0;
    if(!actors||cnt<1||cnt>4000)return 4;
    static uint64_t buf[4096*4];
    uint32_t c2=cnt>4096*4?4096*4:cnt;
    if(rd(actors,buf,(uint64_t)c2*8)!=0)return 5;

    uint64_t prox=0;
    for(uint32_t i=0;i<c2 && !prox;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]=""; if(!cls_name(a,cn,sizeof cn))continue;
        if(strstr(cn,"FastRepProxy")) prox=a;
    }
    if(!prox){printf("no proxy\n");return 6;}
    printf("proxy=0x%llx  采样 60 次 x 100ms\n",(unsigned long long)prox);

    static unsigned char prev[WIN], cur[WIN];
    if(rd(prox,prev,WIN)!=0)return 7;
    int changed[WIN/4]; memset(changed,0,sizeof changed);
    float lastv[WIN/4];
    for(int i=0;i<WIN/4;i++) memcpy(&lastv[i],prev+i*4,4);

    int n = 0;
    for(int t=0;t<60;t++){
        usleep(100000);
        if(rd(prox,cur,WIN)!=0) continue;
        n++;
        for(int i=0;i<WIN/4;i++){
            float v; memcpy(&v,cur+i*4,4);
            if(v!=lastv[i]){ changed[i]++; lastv[i]=v; }
        }
    }
    printf("实际采样 %d 次\n--- 变化次数最多的字段（只列看起来像坐标的）---\n",n);
    for(int i=0;i<WIN/4;i++){
        if(changed[i]<2) continue;
        float x,y,z; memcpy(&x,cur+i*4,4);
        if(i+2 >= WIN/4) continue;
        memcpy(&y,cur+(i+1)*4,4); memcpy(&z,cur+(i+2)*4,4);
        if(!(fabsf(x)>1000.f&&fabsf(x)<1e6f)) continue;
        if(!(fabsf(y)>1000.f&&fabsf(y)<1e6f)) continue;
        if(!(200.f<z&&z<4300.f)) continue;
        printf("  +0x%03x 变化%2d次/60  (%9.1f,%9.1f,%9.1f)\n", i*4, changed[i], x,y,z);
    }
    /* 直接报 0x314 的更新次数 */
    printf("--- +0x314 变化 %d 次 / %d 次采样 ---\n", changed[0x314/4], n);
    return 0;
}
