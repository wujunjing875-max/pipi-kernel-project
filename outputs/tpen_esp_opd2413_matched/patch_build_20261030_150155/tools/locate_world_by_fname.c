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
#define NAME_POOL_OFF 0x10628080ULL
#define BLOCKS_OFF 0x48ULL
struct phys_io{uint32_t pid,pad;uint64_t va,buf,size,pa_out;};struct range{uint64_t lo,hi;};struct nid{const char*n;uint32_t id[32];int k;};
static int fd=-1;static uint32_t pid;static uint64_t base,end,pool;static struct range rr[1024];static unsigned rn;
static struct nid want[]={{"World"},{"Level"},{"GameEngine"},{"GameInstance"},{"GameViewportClient"},{"Engine"},{"PersistentLevel"}};
static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}static int ptr(uint64_t v){v=cl(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}static int inlib(uint64_t v){v=cl(v);return v>=base&&v<end;}
static int wanted_map(const char*t){if(strstr(t,"[anon:libc_malloc]")||strstr(t,"[heap]"))return 1;if(strchr(t,'/')||strchr(t,'['))return 0;return 1;}
static int idmatch(uint32_t id,const char**n){for(unsigned i=0;i<sizeof(want)/sizeof(want[0]);i++)for(int j=0;j<want[i].k;j++)if(want[i].id[j]==id){*n=want[i].n;return 1;}return 0;}
int main(int argc,char**argv){if(argc!=2)return 2;pid=(uint32_t)strtoul(argv[1],0,10);syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");if(!m)return 4;
 while(fgets(line,sizeof line,m)){unsigned long long lo=0,hi=0,off=0;char pm[8]={0};int used=0;if(sscanf(line,"%llx-%llx %7s %llx %*s %*s %n",&lo,&hi,pm,&off,&used)<4)continue;if(strstr(line,"libUE4.so")){if(off==0&&!base)base=lo;if(hi>end)end=hi;}if(!strncmp(pm,"rw-p",4)&&lo>=0x1000000000ULL&&wanted_map(line+used)&&hi>lo&&hi-lo<=0x10000000ULL&&rn<1024)rr[rn++]=(struct range){lo,hi};}fclose(m);
 /* UE's binned allocator reserves a huge -w-p arena in alternating 256 MiB slices. */
 for(uint64_t lo=0x711c000000ULL;lo<=0x751c000000ULL;lo+=0x20000000ULL)
  rr[rn++]=(struct range){lo,lo+0x10000000ULL};
 pool=base+NAME_POOL_OFF;
 uint32_t cur=0,cursor=0;rd(pool+0x40,&cur,4);rd(pool+0x44,&cursor,4);fprintf(stderr,"pid=%u base=0x%llx pool=0x%llx current_block=%u cursor=%u ranges=%u\n",pid,(unsigned long long)base,(unsigned long long)pool,cur,cursor,rn);
 for(uint32_t bi=0;bi<=cur&&bi<8192;bi++){uint64_t bp=0;if(rd(pool+BLOCKS_OFF+(uint64_t)bi*8,&bp,8))continue;bp=cl(bp);if(!ptr(bp))continue;uint32_t lim=bi==cur?cursor:0x20000;uint8_t*b=malloc(lim);if(!b||rd(bp,b,lim)){free(b);continue;}for(uint32_t off=0;off+2<=lim;){uint16_t h=*(uint16_t*)(b+off);int len=(h>>6)&0x3ff,wide=h&1;if(len<1||len>1023){off+=2;continue;}uint32_t bytes=2+(uint32_t)len*(wide?2:1);uint32_t next=(bytes+1)&~1u;if(off+next>lim)break;if(!wide){for(unsigned i=0;i<sizeof(want)/sizeof(want[0]);i++)if((int)strlen(want[i].n)==len&&!memcmp(b+off+2,want[i].n,len)&&want[i].k<32){uint32_t id=(bi<<16)|(off>>1);want[i].id[want[i].k++]=id;printf("fname name=%s id=%u hex=0x%x block=%u off=0x%x\n",want[i].n,id,id,bi,off);}}off+=next;}free(b);}
 uint8_t pg[0x1000];uint64_t named[256];const char*namedn[256];unsigned nn=0,pages=0;
 for(unsigned r=0;r<rn;r++)for(uint64_t at=rr[r].lo;at<rr[r].hi;at+=sizeof pg){uint64_t sz=rr[r].hi-at;if(sz>sizeof pg)sz=sizeof pg;if(rd(at,pg,sz))continue;pages++;for(uint64_t o=0;o+0x20<=sz;o+=8){uint64_t vt=cl(*(uint64_t*)(pg+o));if(!inlib(vt))continue;uint32_t id=*(uint32_t*)(pg+o+0x18);const char*n=0;if(!idmatch(id,&n))continue;uint64_t obj=at+o,q[8]={0};memcpy(q,pg+o,sz-o>=sizeof q?sizeof q:sz-o);printf("named_obj name=%s id=%u obj=0x%llx vt_off=0x%llx flags_index=0x%llx class=0x%llx outer=0x%llx\n",n,id,(unsigned long long)obj,(unsigned long long)(vt-base),(unsigned long long)q[1],(unsigned long long)cl(q[2]),(unsigned long long)cl(q[4]));if(nn<256){named[nn]=obj;namedn[nn]=n;nn++;}}}
 fprintf(stderr,"first_scan pages=%u named_objects=%u\n",pages,nn);
 for(unsigned k=0;k<nn;k++)if(!strcmp(namedn[k],"World")||!strcmp(namedn[k],"GameEngine")||!strcmp(namedn[k],"GameInstance")||!strcmp(namedn[k],"GameViewportClient")){uint64_t target=named[k];for(unsigned r=0;r<rn;r++)for(uint64_t at=rr[r].lo;at<rr[r].hi;at+=sizeof pg){uint64_t sz=rr[r].hi-at;if(sz>sizeof pg)sz=sizeof pg;if(rd(at,pg,sz))continue;for(uint64_t o=0;o+0x40<=sz;o+=8){if(cl(*(uint64_t*)(pg+o+0x10))!=target)continue;uint64_t vt=cl(*(uint64_t*)(pg+o));if(!inlib(vt))continue;uint64_t obj=at+o;printf("instance class_name=%s class=0x%llx obj=0x%llx vt_off=0x%llx name_id=%u outer=0x%llx\n",namedn[k],(unsigned long long)target,(unsigned long long)obj,(unsigned long long)(vt-base),*(uint32_t*)(pg+o+0x18),(unsigned long long)cl(*(uint64_t*)(pg+o+0x20)));}}}
 return 0;}
