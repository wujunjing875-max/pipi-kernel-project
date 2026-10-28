// corr.c —— 常驻相关性扫描：找"更新比代理快"的坐标字段
// 步骤：1) 找加密玩家 2) 预扫 actor 0..0x2400，记下会变的 float
//       3) 200×50ms 跟踪候选 与 代理真值 4) 输出变化次数对比
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
#define ALEN 0x2400
#define MAXC 160
#define NS 200

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
static int cls_name(uint64_t o,char*out,int cap){
    out[0]=0; uint64_t cls=0;
    if(rd(o+0x10,&cls,8)!=0) return 0;
    cls&=0x00ffffffffffffffULL; if(cls<0x10000ULL) return 0;
    uint32_t idx=0; if(rd(cls+0x18,&idx,4)!=0) return 0;
    return fname(idx,out,cap);
}


static int isp(uint64_t v){return v>0x10000000ULL&&v<0x7fffffffffffULL&&(v&7)==0;}
static unsigned char A[0x800], B[0x800];
#define MAXOBJ 40
#define MAXC2 200
static uint64_t objs[MAXOBJ]; static int nobj=0;
static int coff[MAXC2]; static int cobj[MAXC2]; static int chg[MAXC2]; static float lastv[MAXC2]; static int nc=0;

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
    uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
    uint64_t selfpawn=pc?r64(pc+0x380):0;

    uint64_t tgt=0, prox=0;
    for(uint32_t i=0;i<c2 && !tgt;i++){
        uint64_t a=ab[i]&0x00ffffffffffffffULL; if(!a||a==selfpawn)continue;
        char c0[128]=""; if(!cls_name(a,c0,sizeof c0))continue;
        if(!strstr(c0,"Character"))continue;
        uint64_t r2=r64(a+0x158); if(!r2)continue;
        float q[3]; if(rd(r2+0x154,q,12)!=0)continue;
        if(q[2]>=0.f) continue;
        for(uint32_t k=0;k<c2;k++){
            uint64_t pa=ab[k]&0x00ffffffffffffffULL; if(!pa)continue;
            char pc0[128]=""; if(!cls_name(pa,pc0,sizeof pc0))continue;
            if(strstr(pc0,"FastRepProxy") && r64(pa+0x108)==a){ tgt=a; prox=pa; break; }
        }
    }
    if(!tgt){printf("no proxy-backed encrypted player\n");return 6;}
    printf("tgt=0x%llx prox=0x%llx\n",(unsigned long long)tgt,(unsigned long long)prox);

    /* 收集子对象 */
    for(uint64_t o=0x100; o<0x2400 && nobj<MAXOBJ; o+=8){
        uint64_t p=r64(tgt+o);
        if(!isp(p)) continue;
        objs[nobj++]=p;
    }
    printf("子对象 %d 个，预扫 0x800...\n", nobj);
    /* 预扫：子对象内 400ms 内变化的 float 且像坐标 */
    for(int k=0;k<nobj;k++){
        if(rd(objs[k],A,sizeof A)!=0) continue;
        usleep(20000);
        if(rd(objs[k],B,sizeof B)!=0) continue;
        for(int i=0;i+2<(int)sizeof A/4 && nc<MAXC2;i++){
            float a,b; memcpy(&a,A+i*4,4); memcpy(&b,B+i*4,4);
            if(a==b) continue;
            float x,y,z; memcpy(&x,B+i*4,4); memcpy(&y,B+(i+1)*4,4); memcpy(&z,B+(i+2)*4,4);
            if(!(fabsf(x)>500.f&&fabsf(x)<1e6f)) continue;
            if(!(fabsf(y)>500.f&&fabsf(y)<1e6f)) continue;
            if(!(150.f<z&&z<4300.f)) continue;
            coff[nc]=i*4; cobj[nc]=k; chg[nc]=0; lastv[nc]=b; nc++;
        }
    }
    printf("坐标候选 %d 个，跟踪 150×50ms\n", nc);
    static float plast[3]={0}; int pchg=0;
    if(prox) rd(prox+0x314,plast,12);
    for(int t=0;t<150;t++){
        usleep(50000);
        for(int k=0;k<nc;k++){
            float v;
            if(rd(objs[cobj[k]]+coff[k],&v,4)!=0) continue;
            if(v!=lastv[k]){ chg[k]++; lastv[k]=v; }
        }
        if(prox){ float p[3];
            if(rd(prox+0x314,p,12)==0 && (p[0]!=plast[0]||p[1]!=plast[1]||p[2]!=plast[2])){
                pchg++; plast[0]=p[0]; plast[1]=p[1]; plast[2]=p[2]; } }
    }
    printf("\n代理+0x314 变化 %d/150\n--- 变化最多的坐标候选 ---\n", pchg);
    for(int i=0;i<nc;i++) for(int j=i+1;j<nc;j++) if(chg[j]>chg[i]){
        int t=coff[i];coff[i]=coff[j];coff[j]=t;
        t=cobj[i];cobj[i]=cobj[j];cobj[j]=t;
        t=chg[i];chg[i]=chg[j];chg[j]=t;
        float f=lastv[i];lastv[i]=lastv[j];lastv[j]=f; }
    int shown=0;
    for(int k=0;k<nc && shown<8;k++){
        if(chg[k]==0) break;
        char on[96]=""; cls_name(objs[cobj[k]],on,sizeof on);
        printf("  %-26s +0x%04x 变化%3d/150 末值=%.1f\n", on[0]?on:"?", coff[k], chg[k], lastv[k]);
        shown++;
    }
    if(!shown) printf("  (无变化的坐标候选)\n");
    return 0;
}
