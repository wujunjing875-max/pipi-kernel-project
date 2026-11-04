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
#define NAME_BLOCKS_OFF 0x48ULL
struct phys_io{uint32_t pid,pad;uint64_t va,buf,size,pa_out;};
struct range{uint64_t lo,hi;};
struct cent{uint64_t cls;uint32_t idx;char name[96];};
static int fd=-1;static uint32_t pid;static uint64_t base,end,names;
static struct range rr[1024];static unsigned rn;static struct cent cc[65536];static unsigned cn;
static int rd(uint64_t a,void*b,uint64_t n){struct phys_io p={pid,0,a,(uint64_t)(uintptr_t)b,n,0};return ioctl(fd,P_READ_PHYS,&p);}
static uint64_t cl(uint64_t v){return v&0x00ffffffffffffffULL;}
static int ptr(uint64_t v){v=cl(v);return v>=0x1000000000ULL&&v<0x8000000000ULL&&!(v&7);}
static int inlib(uint64_t v){v=cl(v);return v>=base&&v<end;}
static int fname(uint32_t idx,char*out,int cap){out[0]=0;uint32_t bi=idx>>16,off=(idx&0xffffu)*2u;uint64_t b=0;
 if(bi>=8192||rd(names+NAME_BLOCKS_OFF+(uint64_t)bi*8,&b,8))return 0;b=cl(b);if(!ptr(b))return 0;
 uint16_t h=0;if(rd(b+off,&h,2))return 0;int len=(h>>6)&0x3ff,wide=h&1;if(len<1||len>=cap||wide)return 0;
 if(rd(b+off+2,out,(uint64_t)len))return 0;out[len]=0;for(int i=0;i<len;i++)if((unsigned char)out[i]<0x20||(unsigned char)out[i]>0x7e)return 0;return 1;}
static const char* cname(uint64_t cls,uint32_t*idxout){cls=cl(cls);for(unsigned i=0;i<cn;i++)if(cc[i].cls==cls){if(idxout)*idxout=cc[i].idx;return cc[i].name;}
 uint64_t q[4]={0};uint32_t idx=0;char n[96];int ok=ptr(cls)&&rd(cls,q,sizeof q)==0&&inlib(q[0])&&ptr(q[2])&&rd(cls+0x18,&idx,4)==0&&fname(idx,n,sizeof n);
 if(!ok)return 0;if(cn>=65536)return 0;cc[cn].cls=cls;cc[cn].idx=idx;strcpy(cc[cn].name,n);cn++;if(idxout)*idxout=idx;return cc[cn-1].name;}
static int wanted(const char*t){if(strstr(t,"[anon:libc_malloc]")||strstr(t,"[heap]"))return 1;if(strchr(t,'/')||strchr(t,'['))return 0;return 1;}
static int interesting(const char*n){return strstr(n,"World")||strstr(n,"Level")||strstr(n,"GameInstance")||strstr(n,"Engine")||strstr(n,"Viewport")||strstr(n,"GameState");}
int main(int argc,char**argv){if(argc!=2)return 2;pid=(uint32_t)strtoul(argv[1],0,10);syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&fd);if(fd<0)return 3;
 char mp[64],line[1024];snprintf(mp,sizeof mp,"/proc/%u/maps",pid);FILE*m=fopen(mp,"r");if(!m)return 4;
 while(fgets(line,sizeof line,m)){unsigned long long lo=0,hi=0,off=0;char pm[8]={0};int used=0;if(sscanf(line,"%llx-%llx %7s %llx %*s %*s %n",&lo,&hi,pm,&off,&used)<4)continue;
  if(strstr(line,"libUE4.so")){if(off==0&&!base)base=lo;if(hi>end)end=hi;}
  if(!strncmp(pm,"rw-p",4)&&lo>=0x1000000000ULL&&wanted(line+used)&&hi>lo&&hi-lo<=0x10000000ULL&&rn<1024)rr[rn++]=(struct range){lo,hi};}
 fclose(m);names=base+NAME_POOL_OFF;fprintf(stderr,"pid=%u lib=0x%llx-0x%llx names=0x%llx ranges=%u\n",pid,(unsigned long long)base,(unsigned long long)end,(unsigned long long)names,rn);
 uint8_t pg[0x1000];unsigned pages=0,objs=0,hits=0;
 for(unsigned r=0;r<rn;r++)for(uint64_t at=rr[r].lo;at<rr[r].hi;at+=sizeof pg){uint64_t sz=rr[r].hi-at;if(sz>sizeof pg)sz=sizeof pg;if(rd(at,pg,sz))continue;pages++;
  for(uint64_t o=0;o+0x20<=sz;o+=8){uint64_t vt=cl(*(uint64_t*)(pg+o));if(!inlib(vt))continue;uint64_t cls=cl(*(uint64_t*)(pg+o+0x10));if(!ptr(cls))continue;
   uint32_t ci=0;const char*n=cname(cls,&ci);if(!n)continue;objs++;if(!interesting(n))continue;uint64_t obj=at+o;uint32_t oi=*(uint32_t*)(pg+o+0x18);char on[128]="";fname(oi,on,sizeof on);
   uint8_t b[0x380]={0};rd(obj,b,sizeof b);printf("hit obj=0x%llx vt_off=0x%llx cls=0x%llx cls_idx=%u class=%s obj_idx=%u object=%s\n",(unsigned long long)obj,(unsigned long long)(vt-base),(unsigned long long)cls,ci,n,oi,on[0]?on:"-");
   for(unsigned f=0x20;f<=0x360;f+=8){uint64_t p=cl(*(uint64_t*)(b+f));int c=*(int*)(b+f+8),cap=*(int*)(b+f+12);if(ptr(p)&&c>0&&c<10000&&cap>=c&&cap<c*3+128)printf("  tarray +0x%x data=0x%llx count=%d cap=%d\n",f,(unsigned long long)p,c,cap);}hits++;fflush(stdout);
  }}
 fprintf(stderr,"pages=%u named_uobjects=%u classes=%u hits=%u\n",pages,objs,cn,hits);
 for(unsigned i=0;i<cn;i++)printf("class_seen cls=0x%llx idx=%u name=%s\n",(unsigned long long)cc[i].cls,cc[i].idx,cc[i].name);
 return hits?0:1;}
