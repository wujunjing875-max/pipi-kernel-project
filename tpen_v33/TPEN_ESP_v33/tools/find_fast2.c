// find_fast2.c —— 遍历加密角色的所有指针字段，找"会随时间变化"的坐标三元组
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
#define SLOT 64

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
static int cls_name(uint64_t o,char*out,int cap){
    out[0]=0; uint64_t cls=0;
    if(rd(o+0x10,&cls,8)!=0) return 0;
    cls&=0x00ffffffffffffffULL; if(cls<0x10000ULL) return 0;
    uint32_t idx=0; if(rd(cls+0x18,&idx,4)!=0) return 0;
    return fname(idx,out,cap);
}

#define CWIN 0x300
typedef struct { uint64_t obj; int off; float v[3]; int chg; } cand_t;
static cand_t cd[SLOT]; static int cdn=0;

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

    /* 找一个"玩家类但坐标被加密(z<0)"的角色 */
    uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
    uint64_t selfpawn=pc?r64(pc+0x380):0;
    uint64_t tgt=0;
    for(uint32_t i=0;i<c2 && !tgt;i++){
        uint64_t a=ab[i]&0x00ffffffffffffffULL; if(!a)continue;
        if(a==selfpawn) continue;
        { char c0[128]=""; if(!cls_name(a,c0,sizeof c0)) continue;
          if(!strstr(c0,"Character")&&!strstr(c0,"Pawn")) continue;
          if(strstr(c0,"Controller")) continue; }
        /* 目标判据用与绘制一致的口径（加密账号的远端真人可能 f154=24） */
        { uint64_t psx=r64(a+0x330); uint32_t uax=r32(a+0x1290);
          int isp2 = (r32(a+0x154)==50) || (psx>0x10000000ULL&&psx<0x7fffffffffffULL) || (uax>=2&&uax<=3);
          if(!isp2) continue; }
        uint64_t r2=r64(a+0x158); if(!isp(r2))continue;
        float q[3]; if(rd(r2+0x154,q,12)!=0)continue;
        if(fabsf(q[0])<1.f&&fabsf(q[1])<1.f)continue;
        if(q[2] < 0.f) tgt=a;                 /* 加密特征 */
    }
    if(!tgt){ printf("没有加密中的玩家类角色\n"); return 6; }
    char cn[128]=""; cls_name(tgt,cn,sizeof cn);
    printf("target=0x%llx %s\n",(unsigned long long)tgt,cn);

    /* 遍历指针字段，收集"看起来像世界坐标"的三元组 */
    static unsigned char b[CWIN];
    for(uint64_t o=0x100; o<0x2400 && cdn<SLOT; o+=8){
        uint64_t p=r64(tgt+o);
        if(!isp(p)) continue;
        if(rd(p,b,CWIN)!=0) continue;
        for(int i=0;i+2<CWIN/4;i++){
            float x,y,z; memcpy(&x,b+i*4,4); memcpy(&y,b+(i+1)*4,4); memcpy(&z,b+(i+2)*4,4);
            if(!(fabsf(x)>500.f&&fabsf(x)<1e6f)) continue;
            if(!(fabsf(y)>500.f&&fabsf(y)<1e6f)) continue;
            if(!(150.f<z&&z<4300.f)) continue;
            cd[cdn].obj=p; cd[cdn].off=i*4; cd[cdn].v[0]=x; cd[cdn].v[1]=y; cd[cdn].v[2]=z; cd[cdn].chg=0;
            cdn++;
            if(cdn>=SLOT) break;
        }
    }
    printf("候选三元组 %d 个，开始 60×50ms 跟踪\n", cdn);
    for(int t=0;t<60;t++){
        usleep(50000);
        for(int k=0;k<cdn;k++){
            float v[3];
            if(rd(cd[k].obj+cd[k].off,v,12)!=0) continue;
            if(v[0]!=cd[k].v[0]||v[1]!=cd[k].v[1]||v[2]!=cd[k].v[2]){
                cd[k].chg++; cd[k].v[0]=v[0]; cd[k].v[1]=v[1]; cd[k].v[2]=v[2];
            }
        }
    }
    /* 排序输出变化最多的 */
    for(int i=0;i<cdn;i++) for(int j=i+1;j<cdn;j++)
        if(cd[j].chg>cd[i].chg){ cand_t t=cd[i]; cd[i]=cd[j]; cd[j]=t; }
    printf("\n--- 变化最多的候选坐标 ---\n");
    int shown=0;
    for(int k=0;k<cdn && shown<10;k++){
        if(cd[k].chg==0) break;
        char on[96]=""; cls_name(cd[k].obj,on,sizeof on);
        printf("  %-28s +0x%04x 变化 %2d/60  (%9.1f,%9.1f,%9.1f)\n",
            on[0]?on:"?", cd[k].off, cd[k].chg, cd[k].v[0],cd[k].v[1],cd[k].v[2]);
        shown++;
    }
    if(!shown) printf("  (全部静止)\n");
    return 0;
}
