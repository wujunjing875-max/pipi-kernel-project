// paradise_fast45.c — 在 fast18 基础上：
//   ① 容器识别大幅放宽（不再只认 Box/Corpse，加 Inventory/Container/Crate/Cabinet/Bag/... 关键词），
//      且判定顺序改为 盒子 -> 人机 -> 玩家Character -> 其它容器，避免把背袋的 AI 当箱子；
//   ② 新增类名普查：所有"非角色"类名首次出现时追加到 /data/local/tmp/cls_all.txt，
//      现场跑一圈就能拿到真实的容器类名清单（用来补 zhKey 映射）。
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#define M1 0x7a3b9c4du
#define M2 0x2f8e1d6bu
#define P_FIND_PROC 0xC104570Bu
#define P_MODULE_BASE 0xC120570Au
#define P_READ_PHYS 0xC0285709u
#define G_WORLD  0xfbeee10ULL
#define G_ENGINE 0xfba6f20ULL
#define GOFF_GNAMES 0xf987338ULL
static uint64_t GN=0;
struct find_io{uint32_t pid;char name[0x100];};
struct mod_io{uint32_t pid;char name[0x100];uint64_t base;uint32_t size;uint32_t pad;};
struct phys_io{uint32_t pid;uint32_t pad;uint64_t va;uint64_t buf;uint64_t size;uint64_t pa_out;};
static int FD=-1; static uint32_t PID=0;
static uint64_t CTRL_OFF=0;
// ---- 玩家名字：PlayerState + 0x470 是一个 UTF16 字符串指针（RC 同款）----
// 返回 0 表示没读到；名字里的空格/换行统一换成 '_'，避免破坏 esp.txt 的空格分隔格式
static int rd(uint64_t va, void* b, uint64_t n);
static uint64_t r64(uint64_t va);

static int read_pname(uint64_t ps, char *out, int cap){
    out[0]=0;
    if(!ps) return 0;
    // 实测（2026-10-25 15:31，扫 BP_UamPlayerState_C 的 0x300~0x800）：
    //   0x440 / 0x450 / 0x6f8 / 0x718 都是玩家名字（FString 的几份拷贝）
    //   0x470 是别的数据（RC 版本才是 0x470，我们这版不是）
    uint64_t np = r64(ps + 0x440);
    if(np < 0x1000000000ULL || np > 0x7fffffffffffULL) np = r64(ps + 0x450);
    // 兜底：0x470 没命中就在 0x400~0x600 里扫第一个「像 UTF16 字符串」的指针
    if(np < 0x1000000000ULL || np > 0x7fffffffffffULL){
        np = 0;
        for(uint32_t o = 0x430; o < 0x470; o += 8){
            uint64_t c = r64(ps + o);
            if(c < 0x1000000000ULL || c > 0x7fffffffffffULL) continue;
            uint16_t w[4]={0};
            if(rd(c, w, 8)!=0) continue;
            if(w[0]>=0x20 && w[0]<0xD800 && (w[1]==0 || (w[1]>=0x20 && w[1]<0xD800))){ np=c; break; }
        }
    }
    if(!np) return 0;
    uint16_t w[24]; memset(w,0,sizeof w);
    if(rd(np, w, 46)!=0) return 0;
    int n=0;
    for(int i=0;i<23 && w[i];i++){
        uint32_t cp = w[i];
        if(cp < 0x20 || (cp>=0xD800 && cp<=0xDFFF)) break;
        if(cp==' '||cp=='\t') cp='_';
        if(cp < 0x80){ if(n<cap-1) out[n++]=(char)cp; }
        else if(cp < 0x800){
            if(n<cap-3){ out[n++]=(char)(0xC0|(cp>>6)); out[n++]=(char)(0x80|(cp&0x3F)); }
            else break;
        } else {
            if(n<cap-4){ out[n++]=(char)(0xE0|(cp>>12)); out[n++]=(char)(0x80|((cp>>6)&0x3F)); out[n++]=(char)(0x80|(cp&0x3F)); }
            else break;
        }
    }
    out[n]=0;
    return n>0;
}

static int DBGCHR=0;
// 加密判定（Z 轴判据）：加密生效时所有真人 Z 被映射到同一个假平面，
// 与自己(local)的 Z 相差极大（实测 -500 vs +2725，差 3200）；人机 Z 恒正常，可交叉验证。
static double enZsum = 0; static int enZn = 0;
static double aiZsum = 0; static int aiZn = 0;
static float  enZdiff = 0.0f; static int encFlag = -1;
// 加密时游戏把所有真人映射到同一个 Z 平面 → 多人 Z 完全一致（差 < 50 单位）
static float enZmin = 0, enZmax = 0; static int enZn2 = 0;
static uint32_t GS_ARR_OFF=0;   // GameState 里 PlayerArray(TArray) 的偏移（自动学习）
static int REAL_PL=0;           // 从 PlayerArray 读到的真实玩家数（全图，不受 cull 限制）
// 加密判定用：收集本帧所有真人的世界坐标，看它们是否异常聚集
#define ENCN 16
static float enX[ENCN], enY[ENCN], enZ[ENCN];
static int   enN = 0;
// 上一帧的角色世界坐标：用来把「穿在角色身上的装备」从物资里剔掉
// （实测：BP_Vest_Kirasa_C 与 BP_Helmet_SSh68_C 坐标完全相同，就是同一个人机身上那套）
#define CHRN 96
static float prX[CHRN], prY[CHRN], prZ[CHRN];
static int   prN = 0;
static float cuX[CHRN], cuY[CHRN], cuZ[CHRN];
static int   cuN = 0;                       // 角色诊断开关（环境变量 UZI_DBG_CHR=1）
static uint32_t OFF_VEST=0, OFF_HELM=0;   // 首次全扫学到「护甲/头盔 耐久曲线」指针偏移（0=未知）      // APawn::Controller 偏移（运行时自标定）
static int rd(uint64_t va,void* b,uint64_t n){struct phys_io p;memset(&p,0,sizeof p);p.pid=PID;p.va=va;p.buf=(uint64_t)(uintptr_t)b;p.size=n;return ioctl(FD,P_READ_PHYS,&p);}
static uint64_t r64(uint64_t va){uint64_t v=0;return rd(va,&v,8)==0?(v&0x00ffffffffffffffULL):0;}
static uint32_t r32(uint64_t va){uint32_t v=0;return rd(va,&v,4)==0?v:0;}
static float rfl(uint64_t va){float v=0;rd(va,&v,4);return v;}
// FNamePool 在 libUE4 内（base+0xf987338 是结构体本体，不是指针）
//   blocks[] 在 +0x10；公式：block=idx>>16, off=(idx&0xffff)*2, 头部在 off, 串在 off+2
static int fname(uint32_t idx, char *out, int cap){
    out[0]=0; if(!idx) return 0;
    uint32_t blkidx = idx >> 16;
    uint32_t off    = (idx & 0xffff) * 2;
    uint64_t blk = 0;
    if(rd(GN + 0x10 + (uint64_t)blkidx*8, &blk, 8) != 0) return 0;
    blk &= 0x00ffffffffffffffULL;
    if(blk < 0x10000ULL) return 0;
    uint16_t h = 0;
    if(rd(blk + off, &h, 2) != 0) return 0;
    int len = (h >> 6) & 0x3ff;
    if(len < 1 || len >= cap) return 0;
    if(rd(blk + off + 2, out, len) != 0) return 0;
    out[len] = 0;
    for(int i=0;i<len;i++){ unsigned char c=out[i]; if(c<0x20||c>0x7e) return 0; }
    return 1;
}

// 类名：cls = *(actor+0x10)，类名 FName 索引在 cls+0x18
static int cls_name(uint64_t actor, char *out, int cap){
    out[0]=0;
    uint64_t cls = 0;
    if(rd(actor + 0x10, &cls, 8) != 0) return 0;
    cls &= 0x00ffffffffffffffULL;
    if(cls < 0x10000ULL) return 0;
    uint32_t idx = 0;
    if(rd(cls + 0x18, &idx, 4) != 0) return 0;
    return fname(idx, out, cap);
}

static void qrot(float qx,float qy,float qz,float qw,float vx,float vy,float vz,float* ox,float* oy,float* oz){
    float ux=qx,uy=qy,uz=qz;
    float tx=uy*vz-uz*vy, ty=uz*vx-ux*vz, tz=ux*vy-uy*vx;
    *ox=vx+2.0f*(qw*tx + (uy*tz-uz*ty));
    *oy=vy+2.0f*(qw*ty + (uz*tx-ux*tz));
    *oz=vz+2.0f*(qw*tz + (ux*ty-uy*tx));
}
static const int BSEG[28]={15,82,15,1,15,53,53,54,54,87,15,23,23,24,24,86,1,2,2,4,4,92,1,7,7,9,9,94};

// ---------------- 类名判定表 ----------------
static int has_any(const char *s, const char *kw[], int n){
    for(int i=0;i<n;i++) if(strstr(s,kw[i])) return 1;
    return 0;
}
#define NARR(x) ((int)(sizeof(x)/sizeof((x)[0])))
// 死亡盒 / 尸体（玩家盒与人机盒都会命中，具体是谁由绘制端看类名再分）
static const char *BOX_KW[] = { "DeathBox","Corpse","DeadBody","DeadPawn","BodyBox" };
// 人机
static const char *BOT_KW[] = { "AICharacter","AIPawn","Scav","Monster","Bot","Preview","Template","NPC","Npc" };
// 注意：BOT_KW 只在「类名里含 Character/Pawn」时才会被用到，摄像机/捕捉器不会被误判
// 这些名字里带 Character 但不是人形实体的，直接排除
static const char *EXCL_KW[] = { "CameraManager","CaptureActor","PlayerState","GameState",
                                 "HUD","Widget","Menu","Setting","SpawnPoint","Volume" };
// 其它可交互容器（背包/箱子/柜子/抽屉/冰箱/洗衣机/机箱/收银机/文件柜/保险箱…）
static const char *CONT_KW[] = {
    "Inventory","Container","Loot","Crate","Box","Cabinet","Drawer","Locker","Safe",
    "Fridge","Washer","Chest","Suitcase","Briefcase","Duffel","Backpack","Bag","Stash",
    "Case","Wardrobe","Closet","Shelf","Rack","Coffin","Vending","Trash","Barrel",
    "AmmoCan","Toolkit","Pallet","CashRegister","FilingCabinet"
};
// 地上物资（枪械/护甲/弹药/配件/医疗 等），类名通常形如 BP_556x45mm_M855A1_Uam_C
static const char *ITEM_KW[] = {
    "_Uam_C","ItemBase","Helmet","Vest","Headsets","Backpack","Rig","Armor","Ammo",
    "Adapter","Magazine","_Mag","Weapon","Medkit","MedKit","Key_","Case_Item"
};
#define CENSUS_MAX 200
static char SEEN[CENSUS_MAX][64]; static int SEENN=0;
static void census(const char *n){
    if(SEENN>=CENSUS_MAX) return;
    for(int i=0;i<SEENN;i++) if(strcmp(SEEN[i],n)==0) return;
    strncpy(SEEN[SEENN],n,63); SEEN[SEENN][63]=0; SEENN++;
    FILE *g=fopen("/data/local/tmp/cls_all.txt","a");
    if(g){ fprintf(g,"%s\n",n); fclose(g); }
}
// 容器实例名普查（有些容器类名是通用的 BP_ContainerBase_C，类型信息可能只在实例名/子对象里）
#define INST_MAX 200
static char ISEEN[INST_MAX][80]; static int ISEENN=0;
static void census_inst(const char *n){
    if(ISEENN>=INST_MAX) return;
    for(int i=0;i<ISEENN;i++) if(strcmp(ISEEN[i],n)==0) return;
    strncpy(ISEEN[ISEENN],n,79); ISEEN[ISEENN][79]=0; ISEENN++;
    FILE *g=fopen("/data/local/tmp/cont_inst.txt","a");
    if(g){ fprintf(g,"%s\n",n); fclose(g); }
}
// 实例名：actor+0x18 是实例的 FName 索引
static int inst_name(uint64_t actor, char *out, int cap){
    out[0]=0;
    uint32_t idx=0;
    if(rd(actor+0x18,&idx,4)!=0) return 0;
    return fname(idx,out,cap);
}

static uint64_t BUF[4096*8];            // actor 指针数组批量缓冲(128KB)
// ---- 隐蔽处理：伪装进程名 + 抹掉命令行 + 可选自删除 ----
// ps -o NAME 看的是 comm（prctl 改），/proc/pid/cmdline 看的是 argv（这里一起抹）
static void disguise(int argc, char **argv){
    // 先自删除（文件已映射进内存，删掉照样跑；磁盘上就不留 ELF 了）
    if(getenv("UZI_UNLINK")){
        char p[256]; ssize_t n = readlink("/proc/self/exe", p, sizeof p - 1);
        if(n > 0){ p[n] = 0; unlink(p); }
    }
    prctl(PR_SET_NAME, "kworker/u16:2", 0, 0, 0);
    // 进程名伪装后 pidof 找不到它，自己记一份 pid 给脚本用（隐藏文件名）
    { FILE *pf = fopen("/data/local/tmp/.cp", "w"); if(pf){ fprintf(pf, "%d\n", (int)getpid()); fclose(pf); } }
    if(argc > 0 && argv[0]){
        size_t n = strlen(argv[0]);
        memset(argv[0], 0, n);
        const char *fake = "kworker/u16:2";
        if(n >= strlen(fake)) memcpy(argv[0], fake, strlen(fake));
    }
    for(int i = 1; i < argc; i++) if(argv[i]) memset(argv[i], 0, strlen(argv[i]));
}

// adb 侧 su 只有 uid0、没有 caps，杀不掉 root 采集端，所以由新进程自清理
static void kill_other_collectors(void){
    pid_t me=getpid();
    DIR *d=opendir("/proc"); if(!d) return;
    struct dirent *e;
    while((e=readdir(d))){
        if(e->d_name[0]<'0'||e->d_name[0]>'9') continue;
        int pid=atoi(e->d_name);
        if(pid==(int)me) continue;
        char p[64]; snprintf(p,sizeof p,"/proc/%d/cmdline",pid);
        FILE *f=fopen(p,"rb"); if(!f) continue;
        char buf[256]; size_t n=fread(buf,1,sizeof buf-1,f); fclose(f);
        buf[n]=0;
        if(!strstr(buf,"paradise_fast")) continue;
        kill(pid,9);
    }
    closedir(d);
}

// ===================== dump 工具 main =====================
// 用法： uzi_dump <tag>    输出 /data/local/tmp/dump_<tag>.txt
int main(int argc, char **argv){
    char tagbuf[64]; snprintf(tagbuf,sizeof tagbuf,"%s",(argc>1&&argv[1][0])?argv[1]:"snap"); const char *tag = tagbuf;
    disguise(argc, argv);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&FD);
    if(FD < 0){ fprintf(stderr,"no fd\n"); return 1; }
    struct find_io fi; memset(&fi,0,sizeof fi);
    snprintf(fi.name,sizeof fi.name,"com.tencent.mf.uam");
    if(ioctl(FD,P_FIND_PROC,&fi)!=0 || !fi.pid){ fprintf(stderr,"no pid\n"); return 2; }
    PID = fi.pid;
    struct mod_io mo; memset(&mo,0,sizeof mo); mo.pid=PID;
    snprintf(mo.name,sizeof mo.name,"libUE4.so");
    if(ioctl(FD,P_MODULE_BASE,&mo)!=0){ fprintf(stderr,"no base\n"); return 3; }
    uint64_t base = mo.base; GN = base + GOFF_GNAMES;

    char path[160]; snprintf(path,sizeof path,"/data/local/tmp/dump_%s.txt",tag);
    FILE *o = fopen(path,"w");
    if(!o){ fprintf(stderr,"no file %s\n",path); return 4; }

    uint64_t world = r64(base + G_WORLD);
    uint64_t lvl   = r64(world + 0x30);
    uint64_t arr   = r64(lvl + 0x98);
    int cnt = (int)r32(lvl + 0xa0);
    fprintf(o,"# tag=%s pid=%u base=%llx world=%llx cnt=%d\n",
            tag, PID, (unsigned long long)base, (unsigned long long)world, cnt);
    if(cnt<=0 || cnt>4000){ fclose(o); return 5; }

    int np=0, na=0, nps=0;
    static uint64_t BUF[4096];
    int nn = (rd(arr, BUF, (uint64_t)cnt*8)==0) ? cnt : 0;
    if(nn > 4000) nn = 4000;
    for(int i=0;i<nn;i++){
        uint64_t a = BUF[i] & 0x00ffffffffffffffULL;
        if(!a) continue;
        char cn[128]="";
        if(!cls_name(a, cn, sizeof cn)) continue;

        int isP  = (strstr(cn,"BP_UamCharacter_C") != 0);
        int isA  = (strstr(cn,"BP_UamAICharacter_C") != 0);
        int isPS = (strstr(cn,"PlayerState") != 0);
        if(!isP && !isA && !isPS) continue;
        if(isP  && np  >= 12) continue;
        if(isA  && na  >= 3) continue;
        if(isPS && nps >= 6) continue;
        if(isP) np++; else if(isA) na++; else nps++;

        fprintf(o,"\n=== %s actor=%llx\n", cn, (unsigned long long)a);
        for(uint32_t off=0; off<0x2400; off+=4){
            uint32_t v = r32(a + off);
            if(v) fprintf(o,"%04x %08x\n", off, v);
        }
        // PlayerState 额外 dump 名字，便于对齐
        if(isP){
            char pn[64]=""; read_pname(r64(a+0x330), pn, sizeof pn);
            fprintf(o,"# name=%s\n", pn[0]?pn:"-");
        }
    }
    fprintf(stderr,"cnt=%d nn=%d np=%d na=%d nps=%d\n",cnt,nn,np,na,nps);
    fclose(o);
    return 0;
}
