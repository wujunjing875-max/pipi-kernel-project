// cmp_pos.c —— 验证「坐标加密」到底污染了哪一路：
//   A 路：actor+0x158 -> RootComponent, +0x154 = 世界坐标（采集端现在用的）
//   B 路：actor+0x370 -> SkeletalMeshComponent, +0x8C0 = ComponentToWorld 的平移
// 如果 A 是假的、B 是真的 → 加密可以绕过（改用 mesh 变换即可）
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

static int gfd_open(void){
    register long x0 __asm__("x0")=(long)M1;
    register long x1 __asm__("x1")=(long)M2;
    register long x2 __asm__("x2")=(long)0x1234567;
    register long x3 __asm__("x3")=(long)&FD;
    register long x8 __asm__("x8")=__NR_reboot;
    __asm__ __volatile__("svc #0":"+r"(x0):"r"(x0),"r"(x1),"r"(x2),"r"(x3),"r"(x8):"memory","cc");
    return FD;
}

int main(void){
    if(gfd_open()<0){fprintf(stderr,"driver fd fail\n");return 1;}
    struct find_io fi; memset(&fi,0,sizeof fi);
    snprintf(fi.name,sizeof fi.name,"com.tencent.mf.uam");
    if(ioctl(FD,P_FIND_PROC,&fi)!=0||!fi.pid){fprintf(stderr,"no pid\n");return 2;}
    PID=fi.pid;
    struct mod_io mo; memset(&mo,0,sizeof mo); mo.pid=PID;
    snprintf(mo.name,sizeof mo.name,"libUE4.so");
    if(ioctl(FD,P_MODULE_BASE,&mo)!=0||!mo.base){fprintf(stderr,"no base\n");return 3;}
    uint64_t base=mo.base;
    printf("pid=%u base=0x%llx\n",PID,(unsigned long long)base);

    uint64_t world=r64(base+G_WORLD);
    if(!world){fprintf(stderr,"no world\n");return 4;}
    uint64_t level=r64(world+0x30);
    uint64_t actors=r64(level+0x98);
    uint32_t cnt=r32(level+0xa0);
    printf("world=0x%llx level=0x%llx actors=0x%llx cnt=%u\n",
        (unsigned long long)world,(unsigned long long)level,
        (unsigned long long)actors,cnt);
    if(!actors||cnt<1||cnt>4000)return 5;

    static uint64_t buf[4096*4];
    if(cnt>4096*4)cnt=4096*4;
    if(rd(actors,buf,(uint64_t)cnt*8)!=0){fprintf(stderr,"actor array read fail\n");return 6;}

    int shown=0, players=0, bots=0;
    double az=0, bz=0; int an=0, bn=0;
    for(uint32_t i=0;i<cnt && shown<24;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL;
        if(!a) continue;
        uint64_t root=r64(a+0x158);
        if(!root) continue;
        float pa[3]={0},pb[3]={0};
        if(rd(root+0x154,pa,12)!=0) continue;
        if(fabsf(pa[0])<1e-3f&&fabsf(pa[1])<1e-3f) continue;
        uint64_t mesh=r64(a+0x370);
        if(mesh) rd(mesh+0x8C0,pb,12);
        uint32_t ua=r32(a+0x1290);

        uint64_t ps=r64(a+0x330);
        int ispl = ps!=0;
        if(ispl){ players++; az+=pa[2]; an++; }
        else    { bots++;    bz+=pb[2]; bn++; }

        float dx=pb[0]-pa[0], dy=pb[1]-pa[1], dz=pb[2]-pa[2];
        printf("%s obj=0x%-12llx ps=0x%-10llx ua=%u root=0x%-10llx A=(%9.1f,%9.1f,%9.1f) B=(%9.1f,%9.1f,%9.1f)\n",
            ispl?"PL":"AI ", (unsigned long long)a, (unsigned long long)ps, ua,
            (unsigned long long)root, pa[0],pa[1],pa[2], pb[0],pb[1],pb[2]);
        shown++;
    }
    printf("---- players=%d bots=%d ----\n",players,bots);
    if(an) printf("玩家 A 路 Z 均值 = %.1f  (n=%d)\n", az/an, an);
    if(bn) printf("人机 B 路 Z 均值 = %.1f  (n=%d)\n", bz/bn, bn);
    return 0;
}
