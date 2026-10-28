// enc_probe.c —— 加密态对比 + RC 解密链试探 + 人机子类型分布
// 输出：
//   1) 每个玩家类(t154==50)的原始坐标 与 链解出的坐标
//   2) 人机 t614 取值分布（判断现用判据是否过宽）
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
static int rp(uint64_t va,float*p){return rd(va,p,12)==0;}
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
    uint64_t selfpawn=pc?r64(pc+0x380):0;
    uint64_t cm=pc?r64(pc+0x398):0;
    float lp[3]={0}; if(cm) rd(cm+0x1100,lp,12);
    printf("cnt=%u selfpawn=0x%llx local=(%.0f,%.0f,%.0f)\n",
        cnt,(unsigned long long)selfpawn,lp[0],lp[1],lp[2]);

    static uint64_t buf[4096*4];
    if(cnt>4096*4)cnt=4096*4;
    if(rd(actors,buf,(uint64_t)cnt*8)!=0)return 5;

    int hist614[64]; memset(hist614,0,sizeof hist614);
    double pz=0; int pn=0; float pzmin=1e9f,pzmax=-1e9f;
    double bz=0; int bn=0; float bzmin=1e9f,bzmax=-1e9f;

    printf("\n--- 玩家类(t154==50) 原始坐标 / 解密链试探 ---\n");
    int shown=0;
    for(uint32_t i=0;i<cnt;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]="";
        if(!cls_name(a,cn,sizeof cn))continue;
        if(!strstr(cn,"Character")&&!strstr(cn,"Pawn"))continue;
        if(strstr(cn,"Preview")||strstr(cn,"Template")||strstr(cn,"Default"))continue;
        uint64_t root=r64(a+0x158); if(!isp(root))continue;
        float A[3]; if(!rp(root+0x154,A))continue;
        if(fabsf(A[0])<1.f&&fabsf(A[1])<1.f)continue;
        uint32_t t154=r32(a+0x154), t614=r32(a+0x614), ua=r32(a+0x1290);
        if(t154==24){ if(t614<64) hist614[t614]++; bz+=A[2]; bn++; if(A[2]<bzmin)pzmin=0; if(A[2]<bzmin)bzmin=A[2]; if(A[2]>bzmax)bzmax=A[2]; }
        if(t154==50){
            pz+=A[2]; pn++; if(A[2]<pzmin)pzmin=A[2]; if(A[2]>pzmax)pzmax=A[2];
            /* 链试探：root -> [0x100..0x140] -> +0x0 -> +0x90 -> +0x1160 */
            printf("PLAYER %-22s ua=%u t614=%u raw=(%.0f,%.0f,%.0f) %s\n",
                cn,ua,t614,A[0],A[1],A[2],(a==selfpawn)?"SELF":"");
            for(uint32_t o1=0x100;o1<=0x140;o1+=8){
                uint64_t p1=r64(root+o1); if(!isp(p1))continue;
                uint64_t p2=r64(p1); if(!isp(p2))continue;
                uint64_t p3=r64(p2+0x90); if(!isp(p3))continue;
                float B[3]; if(!rp(p3+0x1160,B))continue;
                if(fabsf(B[0])<1.f&&fabsf(B[1])<1.f)continue;
                printf("    +0x%x -> %9.0f,%9.0f,%9.0f\n",o1,B[0],B[1],B[2]);
                shown++;
            }
        }
    }
    printf("\n玩家类 z 均值=%.0f 跨度=%.0f (n=%d)\n", pn?pz/pn:0, pn?pzmax-pzmin:0, pn);
    printf("人机   z 均值=%.0f 跨度=%.0f (n=%d)\n", bn?bz/bn:0, bn?bzmax-bzmin:0, bn);
    printf("\n人机 t614 分布: ");
    for(int k=0;k<64;k++) if(hist614[k]) printf("%d=%d  ",k,hist614[k]);
    printf("\n");
    return 0;
}
