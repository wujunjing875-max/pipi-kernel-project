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
int main(int argc,char**argv){if(argc<3)return 2;uint32_t pid=(uint32_t)strtoul(argv[1],0,0);uint64_t at=strtoull(argv[2],0,0);unsigned n=argc>3?(unsigned)strtoul(argv[3],0,0):256;if(n>0x10000)n=0x10000;
 int fd=-1;syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;uint8_t*b=calloc(1,n);struct phys_io p={pid,0,at,(uint64_t)(uintptr_t)b,n,0};int rc=ioctl(fd,P_READ_PHYS,&p);printf("addr=0x%llx bytes=%u rc=%d\n",(unsigned long long)at,n,rc);
 if(!rc)for(unsigned i=0;i<n;i+=16){printf("%08x  ",i);for(unsigned j=0;j<16&&i+j<n;j++)printf("%02x ",b[i+j]);printf(" ");for(unsigned j=0;j<16&&i+j<n;j++){unsigned c=b[i+j];putchar(c>=32&&c<127?c:'.');}putchar('\n');}free(b);return rc?1:0;}
