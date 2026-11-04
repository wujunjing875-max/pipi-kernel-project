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
#define BUDGET  (1200ULL*1024*1024)   /* 最多扫 400MB */

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
    if(!actors||cnt<1||cnt>4000)return 4;
    static uint64_t ab[4096*4];
    uint32_t c2=cnt>4096*4?4096*4:cnt;
    if(rd(actors,ab,(uint64_t)c2*8)!=0)return 5;

    /* 取第一个"有合法坐标"的代理真值作为特征 */
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
    if(!got){printf("没有可用作特征的代理坐标（无真人/未同步）\n");return 6;}
    printf("特征坐标 = (%.2f, %.2f, %.2f)  来自 proxy=0x%llx\n",
        key[0],key[1],key[2],(unsigned long long)prox);

    /* 逐段扫描 */
    char path[64]; snprintf(path,sizeof path,"/proc/%d/maps",PID);
    FILE *f=fopen(path,"r"); if(!f)return 7;
    char line[512];
    /* ---- 覆盖自检：打印代理/角色所在段，确认本次扫描会覆盖它们 ---- */
    { FILE *g=fopen(path,"r"); char ln[512];
      while(g && fgets(ln,sizeof ln,g)){
          unsigned long long s2,e2;
          if(sscanf(ln,"%llx-%llx",&s2,&e2)!=2) continue;
          if((prox>=s2 && prox<e2)){
              char pm[8]=""; sscanf(ln,"%*llx-%*llx %7s",pm);
              printf("[check] proxy 0x%llx 位于 %llx-%llx perm=%s size=%.1fMB\n",
                  (unsigned long long)prox,s2,e2,pm,(e2-s2)/1048576.0);
          }
      }
      if(g) fclose(g);
    }
    unsigned char *buf=malloc(0x1000);
    uint64_t scanned=0; int hits=0;
    const unsigned char *pat=(const unsigned char*)key;
    while(fgets(line,sizeof line,f) && scanned<BUDGET){
        unsigned long long s,e,off; char perm[8], nm[256]; nm[0]=0;
        int k=sscanf(line,"%llx-%llx %7s %llx %*s %*s %255s",&s,&e,perm,&off,nm);
        if(k<4) continue;
        if(perm[0]!='r'||perm[1]!='w') continue;
        unsigned long long len=e-s;
        if(len > 600ULL*1024*1024) continue;           /* 超大段跳过（放宽容积） */
        for(unsigned long long a=s; a+0x1000<=e && scanned<BUDGET; a+=0x1000){
            if(rd(a,buf,0x1000)!=0) break;
            scanned+=0x1000;
            for(int i=0;i+12<=0x1000;i+=4){
                int ok=1;
                for(int q=0;q<3;q++){ float v; memcpy(&v,buf+i+q*4,4);
                    if(!(v==v)) { ok=0; break; }                 /* 排除 NaN */
                    if(fabsf(v-key[q])>1.0f){ ok=0; break; } }
                if(ok){
                    { float vv[3]; memcpy(vv,buf+i,12);
                      printf("HIT 0x%llx (%s)  (%.1f,%.1f,%.1f)\n", a+i, nm[0]?nm:"anon", vv[0],vv[1],vv[2]); }
                    if(++hits>=20) goto done;
                }
            }
        }
    }
done:
    printf("扫描 %.1f MB，命中 %d 处\n", scanned/1048576.0, hits);
    fclose(f);
    return 0;
}
