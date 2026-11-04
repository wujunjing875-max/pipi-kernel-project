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
struct phys_io { uint32_t pid,pad; uint64_t va,buf,size,pa_out; };
static int fd=-1; static uint32_t pid;
static int rd(uint64_t va,void *buf,uint64_t n){
    struct phys_io p={pid,0,va,(uint64_t)(uintptr_t)buf,n,0};
    return ioctl(fd,P_READ_PHYS,&p);
}
static uint64_t clean(uint64_t v){return v&0x00ffffffffffffffULL;}
static int ptr(uint64_t v){v=clean(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: find_gnames PID\n");return 2;}
    pid=(uint32_t)strtoul(argv[1],0,10);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);
    if(fd<0){fprintf(stderr,"driver fd=%d\n",fd);return 3;}
    char path[64],line[1024]; snprintf(path,sizeof path,"/proc/%u/maps",pid);
    FILE *m=fopen(path,"r"); if(!m){perror("maps");return 4;}
    uint64_t base=0; unsigned hits=0,pages=0;
    while(fgets(line,sizeof line,m)){
        unsigned long long lo=0,hi=0,off=0; char perm[8]={0};
        if(!strstr(line,"libUE4.so"))continue;
        if(sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,perm,&off)!=4)continue;
        if(off==0&&!base)base=lo;
    }
    rewind(m);
    uint8_t page[0x1000];
    while(fgets(line,sizeof line,m)){
        unsigned long long lo=0,hi=0,off=0; char perm[8]={0};
        if(!strstr(line,"libUE4.so"))continue;
        if(sscanf(line,"%llx-%llx %7s %llx",&lo,&hi,perm,&off)!=4)continue;
        if(strncmp(perm,"rw-p",4)||off<0xe000000ULL)continue;
        for(uint64_t at=lo;at<hi;at+=sizeof page){
            uint64_t n=hi-at;if(n>sizeof page)n=sizeof page;
            if(rd(at,page,n)!=0)continue;pages++;
            for(uint64_t i=0;i+8<=n;i+=8){
                uint64_t block=clean(*(uint64_t *)(page+i));
                if(!ptr(block))continue;
                uint8_t h[96]={0}; if(rd(block,h,16)!=0)continue;
                uint16_t hdr=*(uint16_t *)h;
                int len=(hdr>>6)&0x3ff;
                int match=(len==4 && !memcmp(h+2,"None",4)), valid=0;
                uint32_t probe_idx[3]={388,395,414};
                char probe[3][72]; memset(probe,0,sizeof probe);
                for(int pi=0;pi<3;pi++){
                    uint64_t poff=(uint64_t)probe_idx[pi]*2;
                    uint8_t e[72]={0}; if(rd(block+poff,e,sizeof e)!=0)continue;
                    int elen=(*(uint16_t*)e>>6)&0x3ff;
                    if(elen<3||elen>63)continue;
                    int ok=1;for(int k=0;k<elen;k++)if(e[2+k]<0x20||e[2+k]>0x7e){ok=0;break;}
                    if(ok){memcpy(probe[pi],e+2,elen);probe[pi][elen]=0;valid++;}
                }
                if(valid==3) match=1;
                else if(!(len==4 && !memcmp(h+2,"None",4))) match=0;
                if(match){
                    uint64_t slot=at+i,gn=slot-0x10;
                    uint32_t state[4]={0};rd(gn,state,sizeof state);
                    printf("gnames_candidate off=0x%llx gn=0x%llx block0=0x%llx hdr=0x%x names=[%s]|[%s]|[%s] state=%08x,%08x,%08x,%08x\n",
                        (unsigned long long)(gn-base),(unsigned long long)gn,
                        (unsigned long long)block,hdr,probe[0],probe[1],probe[2],state[0],state[1],state[2],state[3]);
                    hits++;
                }
            }
        }
    }
    fclose(m);fprintf(stderr,"base=0x%llx pages=%u hits=%u\n",(unsigned long long)base,pages,hits);
    return hits?0:1;
}
