// vscan.c —— 按值搜索：把代理里的真坐标当特征，全内存找其它副本
// 只读；跳过代码段；限定扫描预算避免过慢
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_FIND_PROC 0xC104570Bu
#define P_MODULE_BASE 0xC120570Au
#define P_READ_PHYS 0xC0285709u
#define G_WORLD  0xfbeee10ULL
#define GOFF_GNAMES 0xf987338ULL
#define NEI (64ULL*1024*1024)   /* 最多扫 400MB */

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



#define MAXH 64
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
    static uint64_t ab[4096*4];
    uint32_t c2=cnt>4096*4?4096*4:cnt;
    if(rd(actors,ab,(uint64_t)c2*8)!=0)return 5;

    float key[3]={0}; int got=0; uint64_t prox=0;
    for(uint32_t i=0;i<c2 && !got;i++){
        uint64_t a=ab[i]&0x00ffffffffffffffULL; if(!a)continue;
        char c0[128]=""; if(!cls_name(a,c0,sizeof c0))continue;
        if(!strstr(c0,"FastRepProxy"))continue;
        for(int ch=0; ch<2 && !got; ch++){
            float v[3]={0};
            if(rd(a+0x314+ch*0x1C,v,12)!=0) continue;
            if(fabsf(v[0])>1000.f && v[2]>200.f && v[2]<4300.f){
                key[0]=v[0]; key[1]=v[1]; key[2]=v[2]; got=1; prox=a;
            }
        }
    }
    if(!got){printf("no feature\n");return 6;}
    printf("proxy=0x%llx key=(%.0f,%.0f,%.0f) 扫描±64MB...\n",(unsigned long long)prox,key[0],key[1],key[2]);
    fflush(stdout);

    unsigned char *buf=malloc(0x1000);
    uint64_t hits[MAXH]; int nh=0;
    uint64_t s0=prox>NEI?prox-NEI:0, e0=prox+NEI;
    for(uint64_t a=s0; a+0x1000<=e0 && nh<MAXH; a+=0x1000){
        if(rd(a,buf,0x1000)!=0) continue;
        for(int i=0;i+12<=0x1000;i+=4){
            int ok=1;
            for(int q=0;q<3;q++){ float v; memcpy(&v,buf+i+q*4,4);
                if(!(v==v)){ok=0;break;}
                if(fabsf(v-key[q])>1.0f){ok=0;break;} }
            if(ok){ hits[nh++]=a+i; if(nh>=MAXH) break; }
        }
    }
    printf("命中 %d 处：\n", nh);
    for(int i=0;i<nh;i++) printf("  +0x%llx\n",(unsigned long long)(hits[i]-prox));
    if(!nh) return 0;

    float last[MAXH][3]; int chg[MAXH]; memset(chg,0,sizeof chg);
    for(int i=0;i<nh;i++) rd(hits[i],last[i],12);
    float pl[3]={0}; int pchg=0; rd(prox+0x314,pl,12);
    for(int t=0;t<120;t++){
        usleep(50000);
        for(int i=0;i<nh;i++){
            float v[3];
            if(rd(hits[i],v,12)!=0) continue;
            if(v[0]!=last[i][0]||v[1]!=last[i][1]||v[2]!=last[i][2]){
                chg[i]++; last[i][0]=v[0];last[i][1]=v[1];last[i][2]=v[2];
            }
        }
        float p[3];
        if(rd(prox+0x314,p,12)==0 && (p[0]!=pl[0]||p[1]!=pl[1]||p[2]!=pl[2])){pchg++;pl[0]=p[0];pl[1]=p[1];pl[2]=p[2];}
    }
    printf("\nproxy+0x314 变化 %d/120\n--- 各副本更新次数 ---\n", pchg);
    for(int i=0;i<nh;i++)
        printf("  +0x%-9llx 变化 %3d  末值=(%.0f,%.0f,%.0f)\n",
            (unsigned long long)(hits[i]-prox), chg[i], last[i][0],last[i][1],last[i][2]);
    return 0;
}
