
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

    /* 收集所有带合法坐标的代理 */
    uint64_t pr[16]; int np=0;
    for(uint32_t i=0;i<c2 && np<16;i++){
        uint64_t a=ab[i]&0x00ffffffffffffffULL; if(!a)continue;
        char c0[128]=""; if(!cls_name(a,c0,sizeof c0))continue;
        if(!strstr(c0,"FastRepProxy"))continue;
        float v[3]={0};
        if(rd(a+0x314,v,12)!=0) continue;
        if(fabsf(v[0])>1000.f && v[2]>200.f && v[2]<4300.f) pr[np++]=a;
        else {
            float w[3]={0};
            if(rd(a+0x330,w,12)==0 && fabsf(w[0])>1000.f && w[2]>200.f && w[2]<4300.f) pr[np++]=a;
        }
    }
    if(!np){printf("无可用代理\n");return 6;}
    printf("代理 %d 个，采 250×20ms（5 秒），统计三通道\n", np);
    const uint32_t OFF[3]={0x314,0x330,0x11050};
    const char*NM[3]={"0x314","0x330","0x11050"};
    for(int k=0;k<np && k<4;k++){
        uint64_t a=pr[k];
        float last[3][3]; int chg[3]={0,0,0}; int eq01=0,eq02=0,eq12=0, bad[3]={0,0,0};
        int n=0;
        for(int t=0;t<250;t++){
            float v[3][3];
            for(int c=0;c<3;c++){
                memset(v[c],0,12);
                if(rd(a+OFF[c],v[c],12)!=0){ bad[c]++; continue; }
                if(!(fabsf(v[c][0])>1000.f && v[c][2]>200.f && v[c][2]<4300.f)) bad[c]++;
            }
            n++;
            if(t==0){ memcpy(last,v,sizeof v); continue; }
            for(int c=0;c<3;c++)
                if(v[c][0]!=last[c][0]||v[c][1]!=last[c][1]||v[c][2]!=last[c][2]){ chg[c]++; memcpy(last[c],v[c],12); }
            /* 两两是否同值 */
            if(fabsf(v[0][0]-v[1][0])<2.f && fabsf(v[0][1]-v[1][1])<2.f) eq01++;
            if(fabsf(v[0][0]-v[2][0])<2.f && fabsf(v[0][1]-v[2][1])<2.f) eq02++;
            if(fabsf(v[1][0]-v[2][0])<2.f && fabsf(v[1][1]-v[2][1])<2.f) eq12++;
            usleep(20000);
        }
        printf("\n--- 代理 %d 0x%llx ---\n", k, (unsigned long long)a);
        for(int c=0;c<3;c++)
            printf("  %-7s 变化 %3d/%d  无效帧 %3d  末值=(%.0f,%.0f,%.0f)\n",
                NM[c], chg[c], n, bad[c], last[c][0],last[c][1],last[c][2]);
        printf("  同值比例: 0x314==0x330 %.0f%%   0x314==0x11050 %.0f%%   0x330==0x11050 %.0f%%\n",
            eq01*100.0/n, eq02*100.0/n, eq12*100.0/n);
    }
    return 0;
}
