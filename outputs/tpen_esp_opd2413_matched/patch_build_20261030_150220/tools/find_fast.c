// find_fast.c —— 找"更新更快"的坐标源（纯读）
// 做法：对 FastRepProxy 及其关联的 SGCharacterProxy，50ms×80 次采样，
//       统计每个 float 的变化次数，并筛出"三连 float 且量级像世界坐标"的候选。
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_FIND_PROC 0xC104570Bu
#define P_MODULE_BASE 0xC120570Au
#define P_READ_PHYS 0xC0285709u
#define G_WORLD  0xfbeee10ULL
#define GOFF_GNAMES 0xf987338ULL
#define WIN 0x1000
#define NS  80

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

static unsigned char bufA[WIN], bufB[WIN];
static int chg[WIN/4];

static void scan(const char *tag, uint64_t obj){
    if(!obj){ return; }
    memset(chg,0,sizeof chg);
    if(rd(obj,bufA,WIN)!=0){ printf("%s read fail\n",tag); return; }
    float last[WIN/4];
    for(int i=0;i<WIN/4;i++) memcpy(&last[i],bufA+i*4,4);
    int n=0;
    for(int t=0;t<NS;t++){
        usleep(50000);
        if(rd(obj,bufB,WIN)!=0) continue;
        n++;
        for(int i=0;i<WIN/4;i++){
            float v; memcpy(&v,bufB+i*4,4);
            if(v!=last[i]){ chg[i]++; last[i]=v; }
        }
    }
    printf("\n=== %s obj=0x%llx 采样%d次(50ms) ===\n",tag,(unsigned long long)obj,n);
    /* 候选：三连 float、像世界坐标、且至少 20% 采样在变 */
    int shown=0;
    for(int i=0;i+2<WIN/4;i++){
        if(chg[i] < n/5) continue;
        float x,y,z; memcpy(&x,bufB+i*4,4); memcpy(&y,bufB+(i+1)*4,4); memcpy(&z,bufB+(i+2)*4,4);
        if(!(fabsf(x)>500.f&&fabsf(x)<1e6f)) continue;
        if(!(fabsf(y)>500.f&&fabsf(y)<1e6f)) continue;
        if(!(150.f<z&&z<4300.f)) continue;
        if(chg[i+1] < n/5 || chg[i+2] < n/5) continue;   /* 三个分量都在变才算坐标 */
        printf("  +0x%04x 变化 %d/%d  (%9.1f,%9.1f,%9.1f)\n", i*4, chg[i], n, x,y,z);
        if(++shown>=8) break;
    }
    if(!shown) printf("  (无高频坐标候选)\n");
    printf("  +0x314 变化 %d/%d   +0x330 变化 %d/%d\n",
        chg[0x314/4], n, chg[0x330/4], n);
}

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
    static uint64_t ab[4096*4];
    uint32_t c2=cnt>4096*4?4096*4:cnt;
    if(rd(actors,ab,(uint64_t)c2*8)!=0)return 5;

    uint64_t fp=0;
    for(uint32_t i=0;i<c2 && !fp;i++){
        uint64_t a=ab[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]=""; if(!cls_name(a,cn,sizeof cn))continue;
        if(strstr(cn,"FastRepProxy")) fp=a;
    }
    if(!fp){printf("no FastRepProxy（场上没有真人）\n");return 6;}
    uint64_t own=r64(fp+0x108);
    uint64_t sp=r64(fp+0x708);          /* 关联的 SGCharacterProxy */
    printf("FastRepProxy=0x%llx owner=0x%llx SGCharacterProxy=0x%llx\n",
        (unsigned long long)fp,(unsigned long long)own,(unsigned long long)sp);
    if(isp(own)){
        uint64_t r2=r64(own+0x158);
        if(isp(r2)){ float q[3]; if(rd(r2+0x154,q,12)==0)
            printf("owner root=(%.1f,%.1f,%.1f)   <-- 若 z<0 即被加密\n",q[0],q[1],q[2]); }
    }
    scan("FastRepProxy", fp);
    if(isp(sp)) scan("SGCharacterProxy", sp);
    if(isp(own)) scan("ownerActor", own);
    return 0;
}
