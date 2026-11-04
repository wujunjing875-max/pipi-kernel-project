#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_READ_PHYS 0xC0285709u
struct phys_io{uint32_t pid,pad;uint64_t va,buf,size,pa_out;};
struct range{uint64_t lo,hi;};
struct hit{uint64_t level,arr;unsigned off;int count;};
static int fd=-1;static uint32_t pid;static uint64_t base,end;
static struct range ranges[1024];static unsigned rn;static struct hit hits[128];static unsigned hn;
static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}
static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}
static int ptr(uint64_t v){v=cl(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}
static int inlib(uint64_t v){v=cl(v);return v>=base&&v<end;}
static int exact_uobject(uint64_t p){
    uint64_t q[4]={0},cvt=0;p=cl(p);
    if(!ptr(p)||rd(p,q,sizeof q)!=0||!inlib(q[0]))return 0;
    uint64_t cls=cl(q[2]);
    if(!ptr(cls)||cls==p||rd(cls,&cvt,8)!=0||!inlib(cvt))return 0;
    return 1;
}
static int vt_object(uint64_t p){
    uint64_t vt=0; p=cl(p);
    return ptr(p)&&!inlib(p)&&rd(p,&vt,8)==0&&inlib(vt);
}
static int actor_array(uint64_t arr,int count,int*out_obj){
    uint64_t a[64]={0};int n=count<64?count:64,ps=0,os=0;
    if(!ptr(arr)||rd(cl(arr),a,(uint64_t)n*8)!=0)return 0;
    for(int i=0;i<n;i++){
        uint64_t p=cl(a[i]);if(!ptr(p)||inlib(p))continue;ps++;
        if(vt_object(p))os++;
    }
    *out_obj=os;
    return ps*2>=n && os>=8 && os*3>=ps*2;
}
static int wanted(const char*tail){
    if(strstr(tail,"[anon:libc_malloc]")||strstr(tail,"[heap]"))return 1;
    if(strchr(tail,'/')||strchr(tail,'['))return 0;
    return 1;
}
static void maps(FILE*m){
    char line[1024];rewind(m);
    while(fgets(line,sizeof line,m)){
        unsigned long long lo=0,hi=0,off=0;char pm[8]={0};int used=0;
        if(sscanf(line,"%llx-%llx %7s %llx %*s %*s %n",&lo,&hi,pm,&off,&used)<4)continue;
        if(strncmp(pm,"rw-p",4)||lo<0x1000000000ULL||!wanted(line+used)||hi<=lo||hi-lo>0x10000000ULL)continue;
        if(rn<1024)ranges[rn++]=(struct range){lo,hi};
    }
}
static void addhit(uint64_t level,uint64_t arr,unsigned off,int count){
    for(unsigned i=0;i<hn;i++)if(hits[i].level==level&&hits[i].off==off)return;
    if(hn<128)hits[hn++]=(struct hit){level,arr,off,count};
}
static void find_levels(void){
    uint8_t pg[0x1000];unsigned pages=0,objs=0;
    for(unsigned r=0;r<rn;r++)for(uint64_t at=ranges[r].lo;at<ranges[r].hi;at+=sizeof pg){
        uint64_t n=ranges[r].hi-at;if(n>sizeof pg)n=sizeof pg;
        if(rd(at,pg,n)!=0)continue;pages++;
        for(uint64_t i=0;i+0x20<=n;i+=8){
            uint64_t vt=cl(*(uint64_t*)(pg+i));if(!inlib(vt))continue;
            uint64_t cls=cl(*(uint64_t*)(pg+i+0x10));if(!ptr(cls))continue;
            uint64_t cvt=0;if(rd(cls,&cvt,8)!=0||!inlib(cvt))continue;
            uint64_t obj=at+i;objs++;
            uint8_t b[0x380]={0};if(rd(obj,b,sizeof b)!=0)continue;
            if(obj==0x70c6a129b0ULL){
                uint64_t da=cl(*(uint64_t*)(b+0xc8));
                int dc=*(int*)(b+0xd0),dp=*(int*)(b+0xd4),dos=0;
                int dok=actor_array(da,dc,&dos);
                fprintf(stderr,"direct_probe obj=0x%llx arr=0x%llx count=%d cap=%d objects=%d ok=%d\n",
                    (unsigned long long)obj,(unsigned long long)da,dc,dp,dos,dok);
            }
            for(unsigned o=0x40;o<=0x360;o+=8){
                uint64_t arr=cl(*(uint64_t*)(b+o));int c=*(int*)(b+o+8),cap=*(int*)(b+o+12);
                if(!ptr(arr)||c<20||c>5000||cap<c||cap>c*2+128)continue;
                int os=0;if(!actor_array(arr,c,&os))continue;
                uint32_t idx=0;rd(cls+0x18,&idx,4);
                printf("level_candidate level=0x%llx class=0x%llx class_idx=%u actors_off=0x%x actors=0x%llx count=%d cap=%d sample_objects=%d\n",
                    (unsigned long long)obj,(unsigned long long)cls,idx,o,(unsigned long long)arr,c,cap,os);
                addhit(obj,arr,o,c);fflush(stdout);
            }
        }
    }
    fprintf(stderr,"level_scan pages=%u uobjects=%u hits=%u\n",pages,objs,hn);
}
static void find_worlds(void){
    uint8_t pg[0x1000];unsigned wh=0;
    for(unsigned r=0;r<rn;r++)for(uint64_t at=ranges[r].lo;at<ranges[r].hi;at+=sizeof pg){
        uint64_t n=ranges[r].hi-at;if(n>sizeof pg)n=sizeof pg;if(rd(at,pg,n)!=0)continue;
        for(uint64_t i=0;i+8<=n;i+=8){uint64_t v=cl(*(uint64_t*)(pg+i));
            for(unsigned h=0;h<hn;h++)if(v==hits[h].level){uint64_t field=at+i;
                for(unsigned o=0x20;o<=0x200;o+=8){if(field<o)break;uint64_t world=field-o;
                    if(exact_uobject(world)){
                        printf("world_candidate world=0x%llx level_off=0x%x level=0x%llx actors_off=0x%x count=%d\n",
                            (unsigned long long)world,o,(unsigned long long)v,hits[h].off,hits[h].count);wh++;
                    }
                }
            }
        }
    }
    fprintf(stderr,"world_hits=%u\n",wh);
}
int main(int argc,char**argv){
    if(argc!=2)return 2;pid=(uint32_t)strtoul(argv[1],0,10);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;
    char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");if(!m)return 4;
    while(fgets(line,sizeof line,m)){unsigned long long lo,hi,off;char pm[8];if(strstr(line,"libUE4.so")&&sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)==4){if(off==0&&!base)base=lo;if(hi>end)end=hi;}}
    maps(m);fclose(m);fprintf(stderr,"pid=%u lib=0x%llx-0x%llx ranges=%u\n",pid,(unsigned long long)base,(unsigned long long)end,rn);
    find_levels();if(hn)find_worlds();return hn?0:1;
}
