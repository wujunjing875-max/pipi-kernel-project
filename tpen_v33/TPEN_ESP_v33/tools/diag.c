// diag.c —— 类名驱动的投影诊断：证明矩阵与坐标是否自洽
// 判据：同一目标 脚(-90) 与 头(+80) 的屏幕 x 差应 < 15px；屏高应与 170/距离 成正比
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
#define G_ENGINE 0xfba6f20ULL
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
    uint16_t h=0;
    if(rd(blk+off,&h,2)!=0) return 0;
    int len=(h>>6)&0x3ff;
    if(len<1||len>=cap) return 0;
    if(rd(blk+off+2,out,len)!=0) return 0;
    out[len]=0;
    for(int i=0;i<len;i++){unsigned char c=out[i]; if(c<0x20||c>0x7e) return 0;}
    return 1;
}
static int cls_name(uint64_t actor,char*out,int cap){
    out[0]=0;
    uint64_t cls=0;
    if(rd(actor+0x10,&cls,8)!=0) return 0;
    cls&=0x00ffffffffffffffULL;
    if(cls<0x10000ULL) return 0;
    uint32_t idx=0;
    if(rd(cls+0x18,&idx,4)!=0) return 0;
    return fname(idx,out,cap);
}

int main(void){
    register long x0 __asm__("x0")=(long)M1;
    register long x1 __asm__("x1")=(long)M2;
    register long x2 __asm__("x2")=(long)0x1234567;
    register long x3 __asm__("x3")=(long)&FD;
    register long x8 __asm__("x8")=__NR_reboot;
    __asm__ __volatile__("svc #0":"+r"(x0):"r"(x0),"r"(x1),"r"(x2),"r"(x3),"r"(x8):"memory","cc");
    if(FD<0){fprintf(stderr,"fd fail\n");return 1;}
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
    uint64_t pov=r64(r64(r64(base+G_ENGINE)+0x20)+0x2a0);
    float M[16]; memset(M,0,sizeof M);
    if(pov) rd(pov,M,64);
    printf("cnt=%u  pov=0x%llx  M[0]=%.4f M[5]=%.4f M[15]=%.4f\n",
        cnt,(unsigned long long)pov,M[0],M[5],M[15]);
    if(!actors||cnt<1||cnt>4000)return 4;

    static uint64_t buf[4096*4];
    if(cnt>4096*4)cnt=4096*4;
    if(rd(actors,buf,(uint64_t)cnt*8)!=0)return 5;

    const float CX=1207.f, CY=540.f;
    int shown=0;
    for(uint32_t i=0;i<cnt&&shown<10;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]="";
        if(!cls_name(a,cn,sizeof cn))continue;
        if(!strstr(cn,"AICharacter")&&!strstr(cn,"UamCharacter"))continue;
        if(strstr(cn,"Preview")||strstr(cn,"Template"))continue;
        uint64_t root=r64(a+0x158); if(!isp(root))continue;
        float A[3]; if(rd(root+0x154,A,12)!=0)continue;
        if(fabsf(A[0])<1.f&&fabsf(A[1])<1.f)continue;
        uint64_t ps=r64(a+0x330);
        int realpl = isp(ps);

        float f[3]={A[0],A[1],A[2]-90.f}, h[3]={A[0],A[1],A[2]+80.f};
        float wf=A[0]*M[3]+A[1]*M[7]+(A[2]-90.f)*M[11]+M[15];
        float wh=A[0]*M[3]+A[1]*M[7]+(A[2]+80.f)*M[11]+M[15];
        float xf=NAN,yf=NAN,xh=NAN,yh=NAN;
        if(wf>1.f){ xf=CX+CX*((f[0]*M[0]+f[1]*M[4]+f[2]*M[8]+M[12])/wf);
                    yf=CY-CY*((f[0]*M[1]+f[1]*M[5]+f[2]*M[9]+M[13])/wf); }
        if(wh>1.f){ xh=CX+CX*((h[0]*M[0]+h[1]*M[4]+h[2]*M[8]+M[12])/wh);
                    yh=CY-CY*((h[0]*M[1]+h[1]*M[5]+h[2]*M[9]+M[13])/wh); }
        float hp=yf-yh, dx=xh-xf;
        printf("%-28s %s A=(%8.0f,%8.0f,%7.0f) w=%7.1f feet=(%6.0f,%6.0f) head=(%6.0f,%6.0f) h=%5.0f dx=%5.1f %s\n",
            cn, realpl?"PL":"AI", A[0],A[1],A[2], wf, xf,yf, xh,yh, hp, dx,
            (wf>1.f&&xf>-50&&xf<2464&&yf>-50&&yf<1130)?"ON":"off");
        shown++;
    }
    return 0;
}
