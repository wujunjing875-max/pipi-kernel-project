// enc_catch.c —— 后台守候：一旦出现"非自己的 t154==50 玩家类 actor"，立刻落盘
// 产出：/data/local/tmp/enc_catch_<n>.txt   （摘要 + 关键字段）
//       /data/local/tmp/enc_catch_<n>.bin   （该 actor 0x0~0x2400 原始内存）
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
#define DUMPLEN 0x2400

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

int main(int argc,char**argv){
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

    static unsigned char dump[DUMPLEN];
    int n=0;
    printf("[catch] start, waiting for remote player...\n"); fflush(stdout);
    for(int tick=0; tick<3600; tick++){        // 最多守 1 小时
        usleep(500000);
        uint64_t world=r64(base+G_WORLD);
        uint64_t level=world?r64(world+0x30):0;
        uint64_t actors=level?r64(level+0x98):0;
        uint32_t cnt=level?r32(level+0xa0):0;
        if(!actors||cnt<1||cnt>4000) continue;
        uint64_t pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
        uint64_t selfpawn=pc?r64(pc+0x380):0;
        uint64_t cm=pc?r64(pc+0x398):0;
        float lp[3]={0}; if(cm) rd(cm+0x1100,lp,12);
        static uint64_t buf[4096*4];
        uint32_t c2=cnt>4096*4?4096*4:cnt;
        if(rd(actors,buf,(uint64_t)c2*8)!=0) continue;
        for(uint32_t i=0;i<c2;i++){
            uint64_t a=buf[i]&0x00ffffffffffffffULL; if(!a||a==selfpawn) continue;
            uint32_t t154=r32(a+0x154);
            if(t154!=50) continue;
            uint64_t root=r64(a+0x158); if(!isp(root))continue;
            float A[3]; if(rd(root+0x154,A,12)!=0)continue;
            if(fabsf(A[0])<1.f&&fabsf(A[1])<1.f)continue;
            char cn[128]=""; cls_name(a,cn,sizeof cn);
            float dx=A[0]-lp[0],dy=A[1]-lp[1],dz=A[2]-lp[2];
            float d=sqrtf(dx*dx+dy*dy+dz*dz)*0.01f;

            char tp[64], bp[64];
            snprintf(tp,sizeof tp,"/data/local/tmp/enc_catch_%d.txt",n);
            snprintf(bp,sizeof bp,"/data/local/tmp/enc_catch_%d.bin",n);
            FILE*f=fopen(tp,"w");
            if(f){
                fprintf(f,"obj=0x%llx cls=%s dist=%.1fm\n",(unsigned long long)a,cn,d);
                fprintf(f,"local=(%.1f,%.1f,%.1f)\n",lp[0],lp[1],lp[2]);
                fprintf(f,"raw_pos=(%.1f,%.1f,%.1f)\n",A[0],A[1],A[2]);
                fprintf(f,"ua=%u t154=%u t614=%u t618=%u ps=0x%llx root=0x%llx\n",
                    r32(a+0x1290),t154,r32(a+0x614),r32(a+0x618),
                    (unsigned long long)r64(a+0x330),(unsigned long long)root);
                /* 链试探：root -> [0x100..0x160] -> +0 -> +0x90 -> +0x1150..0x1170 */
                for(uint32_t o1=0x100;o1<=0x160;o1+=8){
                    uint64_t p1=r64(root+o1); if(!isp(p1))continue;
                    uint64_t p2=r64(p1); if(!isp(p2))continue;
                    uint64_t p3=r64(p2+0x90); if(!isp(p3))continue;
                    for(uint32_t o3=0x1140;o3<=0x1180;o3+=0x10){
                        float B[3]; if(rd(p3+o3,B,12)!=0)continue;
                        if(fabsf(B[0])<1.f&&fabsf(B[1])<1.f)continue;
                        fprintf(f,"chain +0x%x/+0x%x -> (%.0f,%.0f,%.0f)\n",o1,o3,B[0],B[1],B[2]);
                    }
                }
                fclose(f);
            }
            if(rd(a,dump,DUMPLEN)==0){
                FILE*g=fopen(bp,"wb"); if(g){ fwrite(dump,1,DUMPLEN,g); fclose(g); }
            }
            printf("[catch] HIT #%d %s d=%.0fm raw=(%.0f,%.0f,%.0f) -> %s\n",
                n,cn,d,A[0],A[1],A[2],tp); fflush(stdout);
            n++;
            if(n>=8) return 0;
            break;
        }
    }
    printf("[catch] timeout, hits=%d\n",n);
    return 0;
}
