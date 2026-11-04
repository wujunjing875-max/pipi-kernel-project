// find_type.c —— 用"最近的人机=特遣"做差分，找出能区分特遣/普通人机的字段
// 思路：对 0..0x2400 每个 4 字节偏移，若「最近这个」的值与其余所有人机都不同，
//       说明它是能区分两者的候选字段。
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
    blk&=0x00ffffffffffffffULL;
    if(blk<0x10000ULL) return 0;
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

#define MAXB 64
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
    uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
    uint64_t cm=pc?r64(pc+0x398):0;
    float lp[3]={0}; if(cm) rd(cm+0x1100,lp,12);

    static uint64_t buf[4096*4];
    if(cnt>4096*4)cnt=4096*4;
    if(rd(actors,buf,(uint64_t)cnt*8)!=0)return 5;

    uint64_t bots[MAXB]; float bd[MAXB]; int nb=0;
    for(uint32_t i=0;i<cnt && nb<MAXB;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]="";
        if(!cls_name(a,cn,sizeof cn))continue;
        if(!strstr(cn,"Character")&&!strstr(cn,"Pawn"))continue;
        if(strstr(cn,"Preview")||strstr(cn,"Template")||strstr(cn,"Default"))continue;
        uint32_t ua=r32(a+0x1290);
        if(ua!=1) continue;                       // 只取人机
        uint64_t root=r64(a+0x158); if(!isp(root))continue;
        float A[3]; if(rd(root+0x154,A,12)!=0)continue;
        if(fabsf(A[0])<1.f&&fabsf(A[1])<1.f)continue;
        float dx=A[0]-lp[0],dy=A[1]-lp[1],dz=A[2]-lp[2];
        bots[nb]=a; bd[nb]=sqrtf(dx*dx+dy*dy+dz*dz)*0.01f; nb++;
    }
    printf("bots=%d\n",nb);
    if(nb<3){fprintf(stderr,"need >=3 bots\n");return 6;}
    // 找最近的（= 特遣）
    int ti=0; for(int i=1;i<nb;i++) if(bd[i]<bd[ti]) ti=i;
    printf("最近的人机 idx=%d dist=%.0fm obj=0x%llx  <-- 假定为特遣\n",
        ti,bd[ti],(unsigned long long)bots[ti]);

    // 差分：候选偏移
    int hits=0;
    for(uint64_t off=0x0; off<0x2400; off+=4){
        uint32_t v0=r32(bots[ti]+off);
        int diff=1, same=0;
        for(int k=0;k<nb;k++){
            if(k==ti) continue;
            uint32_t v=r32(bots[k]+off);
            if(v==v0){ diff=0; break; }
            if(v!=0) same=1;
        }
        if(!diff||!same) continue;
        if(v0==0) continue;
        printf("  off=0x%-5llx 特遣=%u(%d)  其它样本:",(unsigned long long)off,v0,(int)v0);
        for(int k=0;k<nb&&k<6;k++) if(k!=ti) printf(" %u",r32(bots[k]+off));
        printf("\n");
        if(++hits>=25) break;
    }
    // 顺便打印文档里提到的 0x154 / 0x614
    printf("--- 参考字段 ---\n");
    printf("0x154: 特遣=%u 其它:",r32(bots[ti]+0x154));
    for(int k=0;k<nb&&k<6;k++) if(k!=ti) printf(" %u",r32(bots[k]+0x154));
    printf("\n0x614: 特遣=%u 其它:",r32(bots[ti]+0x614));
    for(int k=0;k<nb&&k<6;k++) if(k!=ti) printf(" %u",r32(bots[k]+0x614));
    printf("\n");
    return 0;
}
