// mat_chk.c —— 验证投影矩阵链 base+G_ENGINE -> +0x20 -> +0x2a0
// 检查：16 个 float 是否构成合法 VP（三行正交、w 与距离成正比）
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

struct find_io{uint32_t pid;char name[0x100];};
struct mod_io{uint32_t pid;char name[0x100];uint64_t base;uint32_t size;uint32_t pad;};
struct phys_io{uint32_t pid;uint32_t pad;uint64_t va;uint64_t buf;uint64_t size;uint64_t pa_out;};
static int FD=-1; static uint32_t PID=0;
static int rd(uint64_t va,void* b,uint64_t n){
    struct phys_io p; memset(&p,0,sizeof p);
    p.pid=PID; p.va=va; p.buf=(uint64_t)(uintptr_t)b; p.size=n;
    return ioctl(FD,P_READ_PHYS,&p);
}
static uint64_t r64(uint64_t va){uint64_t v=0;return rd(va,&v,8)==0?(v&0x00ffffffffffffffULL):0;}
static uint32_t r32(uint64_t va){uint32_t v=0;return rd(va,&v,4)==0?v:0;}

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
    uint64_t base=mo.base;

    uint64_t eng=r64(base+G_ENGINE);
    printf("base=0x%llx  engine=0x%llx\n",(unsigned long long)base,(unsigned long long)eng);
    if(!eng)return 4;
    uint64_t e2=r64(eng+0x20);
    printf("engine+0x20 = 0x%llx\n",(unsigned long long)e2);
    if(!e2)return 5;
    uint64_t pov=r64(e2+0x2a0);
    printf("engine+0x20+0x2a0 = 0x%llx   <-- 矩阵地址\n",(unsigned long long)pov);
    if(!pov)return 6;

    float M[16]; memset(M,0,sizeof M);
    if(rd(pov,M,64)!=0){fprintf(stderr,"matrix read fail\n");return 7;}
    printf("M[0..3]  %10.4f %10.4f %10.4f %10.4f\n",M[0],M[1],M[2],M[3]);
    printf("M[4..7]  %10.4f %10.4f %10.4f %10.4f\n",M[4],M[5],M[6],M[7]);
    printf("M[8..11] %10.4f %10.4f %10.4f %10.4f\n",M[8],M[9],M[10],M[11]);
    printf("M[12..15]%10.4f %10.4f %10.4f %10.4f\n",M[12],M[13],M[14],M[15]);

    float r0[3]={M[0],M[4],M[8]}, r1[3]={M[1],M[5],M[9]}, r3[3]={M[3],M[7],M[11]};
    float l0=sqrtf(r0[0]*r0[0]+r0[1]*r0[1]+r0[2]*r0[2]);
    float l1=sqrtf(r1[0]*r1[0]+r1[1]*r1[1]+r1[2]*r1[2]);
    float l3=sqrtf(r3[0]*r3[0]+r3[1]*r3[1]+r3[2]*r3[2]);
    printf("|row0|=%.4f |row1|=%.4f |w row|=%.4f\n",l0,l1,l3);
    if(l0>1e-6f&&l1>1e-6f){
        float d=(r0[0]*r1[0]+r0[1]*r1[1]+r0[2]*r1[2])/(l0*l1);
        printf("row0·row1 (归一化) = %.4f   （应为 0）\n",d);
    }
    /* 用自机位置验证：w 应该等于相机到该点的前向距离 */
    uint64_t world=r64(base+G_WORLD);
    uint64_t level=r64(world+0x30);
    uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
    uint64_t cm=pc?r64(pc+0x398):0;
    float lp[3]={0};
    if(cm) rd(cm+0x1100,lp,12);
    printf("pc=0x%llx camMan=0x%llx pov.loc=(%.0f,%.0f,%.0f)\n",
        (unsigned long long)pc,(unsigned long long)cm,lp[0],lp[1],lp[2]);

    uint32_t cnt=r32(level+0xa0);
    uint64_t actors=r64(level+0x98);
    printf("actors cnt=%u\n",cnt);
    if(actors&&cnt>0&&cnt<4000){
        static uint64_t buf[4096*4];
        if(rd(actors,buf,(uint64_t)cnt*8)==0){
            int shown=0;
            for(uint32_t i=0;i<cnt&&shown<6;i++){
                uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
                uint64_t root=r64(a+0x158); if(!root)continue;
                float q[3]; if(rd(root+0x154,q,12)!=0)continue;
                if(fabsf(q[0])<1.0f&&fabsf(q[1])<1.0f)continue;
                float w=q[0]*M[3]+q[1]*M[7]+q[2]*M[11]+M[15];
                float sx=1207.f+1207.f*((q[0]*M[0]+q[1]*M[4]+q[2]*M[8]+M[12])/w);
                float sy=540.f-540.f*((q[0]*M[1]+q[1]*M[5]+q[2]*M[9]+M[13])/w);
                printf("  a=0x%llx q=(%8.1f,%8.1f,%8.1f) w=%9.1f -> (%7.1f,%7.1f) %s\n",
                    (unsigned long long)a,q[0],q[1],q[2],w,sx,sy,
                    (w>1.f&&sx>-100&&sx<2514&&sy>-100&&sy<1180)?"ON":"off");
                shown++;
            }
        }
    }
    return 0;
}
