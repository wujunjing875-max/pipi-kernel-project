// find_ua.c —— 自标定分类枚举偏移
// 判据：对全部 Character actor，某偏移的 uint32 取值只能落在 {1,2,3}，
//       且"本地自己"那个 actor 的值必须是 3，且至少出现 2 个不同取值。
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
    if(!actors||cnt<1||cnt>4000){fprintf(stderr,"bad world\n");return 4;}

    uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
    uint64_t selfpawn=pc?r64(pc+0x380):0;
    printf("pc=0x%llx self pawn=0x%llx cnt=%u\n",
        (unsigned long long)pc,(unsigned long long)selfpawn,cnt);

    static uint64_t buf[4096*4];
    if(cnt>4096*4)cnt=4096*4;
    if(rd(actors,buf,(uint64_t)cnt*8)!=0)return 5;

    // 收集所有 Character actor
    static uint64_t chr[512]; static char cname[512][64]; static int cps[512];
    int nch=0, selfidx=-1;
    for(uint32_t i=0;i<cnt && nch<512;i++){
        uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a)continue;
        char cn[128]="";
        if(!cls_name(a,cn,sizeof cn))continue;
        if(!strstr(cn,"Character")&&!strstr(cn,"Pawn"))continue;
        if(strstr(cn,"Preview")||strstr(cn,"Template")||strstr(cn,"Default"))continue;
        if(strstr(cn,"PlayerState")||strstr(cn,"Controller")||strstr(cn,"CameraManager"))continue;
        chr[nch]=a; snprintf(cname[nch],64,"%s",cn);
        cps[nch]= isp(r64(a+0x330));
        if(a==selfpawn) selfidx=nch;
        nch++;
    }
    printf("characters=%d  selfidx=%d\n",nch,selfidx);
    if(nch<3){fprintf(stderr,"too few characters\n");return 6;}

    // 扫描候选偏移
    int hits=0;
    for(uint64_t off=0x800; off<0x2400; off+=4){
        int good=0, distinct[4]={0};
        int selfv=-1;
        for(int k=0;k<nch;k++){
            uint32_t v=r32(chr[k]+off);
            if(v>=1&&v<=3){ good++; distinct[v]=1; }
            if(k==selfidx) selfv=(int)v;
        }
        if(good < nch*8/10) continue;                    // 绝大多数必须落在 1..3
        int nd = (distinct[1]?1:0)+(distinct[2]?1:0)+(distinct[3]?1:0);
        if(nd < 2) continue;                             // 至少两种取值
        if(selfidx>=0 && selfv!=3) continue;             // 自己必须是 3
        printf("CANDIDATE off=0x%llx good=%d/%d distinct=%d (1:%d 2:%d 3:%d)\n",
            (unsigned long long)off, good, nch, nd, distinct[1],distinct[2],distinct[3]);
        // 打印前几个样本
        for(int k=0;k<nch&&k<6;k++)
            printf("    %-28s v=%u ps=%d\n", cname[k], r32(chr[k]+off), cps[k]);
        if(++hits>=6) return 0;
    }
    printf("no candidate\n");
    return 0;
}
