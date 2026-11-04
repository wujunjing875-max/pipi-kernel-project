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
static int fd=-1;static uint32_t pid;
static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}
static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}
static int ptr(uint64_t v){v=cl(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}
static void hex16(char*out,const uint8_t*b){for(int i=0;i<16;i++)sprintf(out+i*2,"%02x",b[i]);}
int main(int argc,char**argv){
 if(argc!=2)return 2;pid=(uint32_t)strtoul(argv[1],0,10);syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;
 char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");if(!m)return 4;
 uint64_t base=0;unsigned hits=0,pages=0;while(fgets(line,sizeof line,m)){unsigned long long lo,hi,off;char pm[8];if(strstr(line,"libUE4.so")&&sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)==4&&off==0&&!base)base=lo;}
 rewind(m);uint8_t pg[0x1000];
 while(fgets(line,sizeof line,m)){unsigned long long lo=0,hi=0,off=0;char pm[8]={0};if(!strstr(line,"libUE4.so")||sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)!=4||strncmp(pm,"rw-p",4)||off<0xf000000ULL)continue;
  for(uint64_t at=lo;at<hi;at+=sizeof pg){uint64_t n=hi-at;if(n>sizeof pg)n=sizeof pg;if(rd(at,pg,n)!=0)continue;pages++;
   for(uint64_t i=0;i+0x38<=n;i+=8){uint32_t cb=*(uint32_t*)(pg+i+8),cur=*(uint32_t*)(pg+i+12);uint64_t b0=cl(*(uint64_t*)(pg+i+16)),b1=cl(*(uint64_t*)(pg+i+24)),b2=cl(*(uint64_t*)(pg+i+32));
    if(cb>4096||cur>=0x10000||!ptr(b0))continue;if(cb>=1&&!ptr(b1))continue;if(cb>=2&&!ptr(b2))continue;
    uint8_t h0[16]={0},h1[16]={0};if(rd(b0,h0,16)!=0)continue;if(cb>=1&&rd(b1,h1,16)!=0)continue;
    int nz0=0,nz1=0;for(int k=0;k<16;k++){if(h0[k])nz0++;if(h1[k])nz1++;}if(nz0<2||(cb>=1&&nz1<2))continue;
    char x0[33]={0},x1[33]={0};hex16(x0,h0);hex16(x1,h1);
    printf("pool_candidate off=0x%llx addr=0x%llx current_block=%u cursor=%u b0=0x%llx b1=0x%llx b2=0x%llx h0=%s h1=%s\n",
      (unsigned long long)(at+i-base),(unsigned long long)(at+i),cb,cur,(unsigned long long)b0,(unsigned long long)b1,(unsigned long long)b2,x0,x1);hits++;
   }
  }
 }
 fclose(m);fprintf(stderr,"base=0x%llx pages=%u hits=%u\n",(unsigned long long)base,pages,hits);return hits?0:1;
}
