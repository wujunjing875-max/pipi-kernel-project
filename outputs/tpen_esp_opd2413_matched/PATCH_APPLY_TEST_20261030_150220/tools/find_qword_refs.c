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
static int fd=-1;static uint32_t pid;static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}
int main(int argc,char**argv){if(argc<3)return 2;pid=(uint32_t)strtoul(argv[1],0,0);uint64_t target=cl(strtoull(argv[2],0,0));syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");if(!m)return 4;unsigned hits=0,pages=0;uint8_t pg[0x1000];
 while(fgets(line,sizeof line,m)){unsigned long long lo,hi,off;char pm[8];if(!strstr(line,"libUE4.so")||sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,pm,&off)!=4)continue;for(uint64_t at=lo;at<hi;at+=sizeof pg){uint64_t n=hi-at;if(n>sizeof pg)n=sizeof pg;if(rd(at,pg,n))continue;pages++;for(uint64_t i=0;i+8<=n;i+=8)if(cl(*(uint64_t*)(pg+i))==target){uint64_t q[8]={0};uint64_t ca=(at+i>=16)?at+i-16:at+i;rd(ca,q,sizeof q);printf("ref addr=0x%llx module_off=0x%llx map_off=0x%llx perms=%s ctx=",(unsigned long long)(at+i),(unsigned long long)(at+i-lo+off),(unsigned long long)off,pm);for(int k=0;k<8;k++)printf(" %016llx",(unsigned long long)q[k]);putchar('\n');hits++;}}}
 fclose(m);fprintf(stderr,"target=0x%llx pages=%u hits=%u\n",(unsigned long long)target,pages,hits);return hits?0:1;}
