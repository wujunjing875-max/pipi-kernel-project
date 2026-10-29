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
#include <sys/stat.h>
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
// 加密判定：只统计已读到的世界 Z（不增加任何额外内存读取）
static double enZsum2 = 0; static int enZn2 = 0;
static double aiZsum2 = 0; static int aiZn2 = 0;
static float  enZspan2 = 0, enZmin2 = 0, enZmax2 = 0;
// ACE 工作目录实时监控（纯文件 stat，不碰游戏内存）
// 加密由腾讯 ACE 反作弊触发，它一旦判定异常就会往 ano_tmp/ 写数据。
// 这条信号【不需要真人在场】，大厅/空场景同样有效。
static long aceScan = 0; static int aceN = 0; static char aceName[64] = "";
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

static uint64_t BUF[4096*8];
/* ---- 解密开关（由悬浮窗面板写 /data/local/tmp/ov_dec.txt，内容 1/0）---- */
static int   g_dec_on = 1;    /* 解密1：代理体真坐标 + 预测（v17 策略） */
static int   g_dec2_on = 0;   /* 解密2：+0x330 通道，原始值直出（不预测） */
static int   g_dec3_on = 0;   /* 解密3：+0x11050 通道（邻域扫描发现的第三份副本），带预测 */
static int   g_dec4_on = 0;   /* 解密4：自动在三通道里挑"最快最新"的一路 */
/* 加密状态锁存：本局检测到一次加密就保持住（换局时复位），
 * 避免"附近暂时没有玩家类对象"时状态跳回未知。 */
static int   g_enc_latch = 0;
/* 代理坐标的实测更新间隔(ms)，0=未知。插值窗口按它自适应。 */
static int   g_pdt_ms = 0;
/* 解密2/3 用"插值"而非"外推"：指数逼近最新样本，平滑且不过冲。
 * tau 越大越平滑越滞后：解密2=0.08s（仅消抖），解密3=0.25s（明显平滑）。 */
static float g_tau = 0.f;
static int   g_enc_ok_frames = 0;
static uint64_t g_enc_world = 0;
static double g_dec_t = 0;
static void dec_reload(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    double now = ts.tv_sec + ts.tv_nsec*1e-9;
    if(now - g_dec_t < 0.5) return;
    g_dec_t = now;
    FILE *f = fopen("/data/local/tmp/ov_dec.txt","r");
    if(f){ int v=1; if(fscanf(f,"%d",&v)==1) g_dec_on=(v!=0); fclose(f); }
    FILE *f2 = fopen("/data/local/tmp/ov_dec2.txt","r");
    if(f2){ int v2=0; if(fscanf(f2,"%d",&v2)==1) g_dec2_on=(v2!=0); fclose(f2); }
    FILE *f3 = fopen("/data/local/tmp/ov_dec3.txt","r");
    if(f3){ int v3=0; if(fscanf(f3,"%d",&v3)==1) g_dec3_on=(v3!=0); fclose(f3); }
    FILE *f4 = fopen("/data/local/tmp/ov_dec4.txt","r");
    if(f4){ int v4=0; if(fscanf(f4,"%d",&v4)==1) g_dec4_on=(v4!=0); fclose(f4); }
    /* 解密4 用较轻的插值（0.12s）：数据够快，不需要太重平滑 */
    g_tau = g_dec4_on ? 0.12f : (g_dec3_on ? 0.25f : (g_dec2_on ? 0.08f : 0.f));
}
/* ---- 代理坐标平滑（跨帧状态）----
 * 网络复制代理只有 10~20Hz，直接读会"一跳一跳"。
 * 这里做速度外推 + 轻平滑：仍是纯读，不改游戏内存。 */
#define SMN 32
typedef struct { uint64_t own; float px,py,pz; float vx,vy,vz; double t_last; float ox,oy,oz; int ok;
                 double t_chg; double dt_last; double t_read; double t_out; int ch; int ch_bad;
                 float l0[3], l1[3], l2[3]; int c0, c1, c2c, win; double t_new[3]; float pick_last[3]; } sm_t;
static sm_t g_sm[SMN];
static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec*1e-9; }
static sm_t* sm_slot(uint64_t own){
    for(int i=0;i<SMN;i++) if(g_sm[i].ok && g_sm[i].own==own) return &g_sm[i];
    for(int i=0;i<SMN;i++) if(!g_sm[i].ok){ g_sm[i].ok=1; g_sm[i].own=own; g_sm[i].t_last=0; return &g_sm[i]; }
    return &g_sm[0];
}
static void sm_update(sm_t*st, float x,float y,float z, float*ox,float*oy,float*oz){
    double t=now_s();
    double dt_out = (st->t_out>0.0) ? (t-st->t_out) : 0.016;
    if(dt_out > 0.5) dt_out = 0.5;
    st->t_out = t;
    if(st->t_last==0.0){ st->px=x; st->py=y; st->pz=z; st->vx=st->vy=st->vz=0; st->t_last=t;
                         st->ox=x; st->oy=y; st->oz=z; *ox=x; *oy=y; *oz=z; return; }
    /* ---- 解密2/3：指数插值（不平滑外推，天然不过冲）---- */
    if(g_tau > 0.f){
        float dxn=x-st->px, dyn=y-st->py, dzn=z-st->pz;
        if(dxn*dxn+dyn*dyn+dzn*dzn > 1.0f){ st->px=x; st->py=y; st->pz=z; st->t_last=t; }
        float a = 1.0f - expf(-(float)dt_out/g_tau);
        if(a>1.f) a=1.f; if(a<0.f) a=0.f;
        st->ox += (st->px - st->ox)*a;
        st->oy += (st->py - st->oy)*a;
        st->oz += (st->pz - st->oz)*a;
        *ox=st->ox; *oy=st->oy; *oz=st->oz;
        return;
    }
    float dx=x-st->px, dy=y-st->py, dz=z-st->pz;
    float move2 = dx*dx+dy*dy+dz*dz;
    if(move2 > 1.0f){                             /* 收到新样本 */
        double dt=t-st->t_last; if(dt<0.01) dt=0.01; if(dt>2.0) dt=2.0;
        /* 记录"两次真实变化之间的间隔"，用于上报更新频率 */
        if(st->t_chg>0) st->dt_last = t - st->t_chg;
        st->t_chg = t;
        float nvx=dx/(float)dt, nvy=dy/(float)dt, nvz=dz/(float)dt;
        float sp=sqrtf(nvx*nvx+nvy*nvy+nvz*nvz), MAXV=1200.f;
        if(sp>MAXV){ float k=MAXV/sp; nvx*=k; nvy*=k; nvz*=k; }
        /* 速度更信新样本（0.7），跟得上转向；否则来回漂 */
        st->vx=st->vx*0.3f+nvx*0.7f; st->vy=st->vy*0.3f+nvy*0.7f; st->vz=st->vz*0.3f+nvz*0.7f;
        st->px=x; st->py=y; st->pz=z; st->t_last=t;
    }
    /* 外推窗口按"实测更新间隔"自适应：窗口 ≈ 1.3×实测间隔，
     * 这样样本到达前刚好补满，既不会半路冻住，也不会过度外推导致过冲。 */
    /* 解密1 采用 v17 的固定窗口（用户要求：保证流畅度，避免自适应抖动） */
    double win = 1.2;
    double el=t-st->t_last; if(el>win) el=win;
    /* 速度随时间衰减，避免长时间外推飞出去 */
    float decay = 1.0f - (float)(el/win)*0.55f;
    float ex=st->px+st->vx*(float)el*decay, ey=st->py+st->vy*(float)el*decay, ez=st->pz+st->vz*(float)el*decay;
    /* 突变吸附：预测值与新样本差距过大时直接贴上去，别慢慢挪 */
    float jx=ex-st->ox, jy=ey-st->oy, jz=ez-st->oz;
    if(jx*jx+jy*jy+jz*jz > 90000.f){ st->ox=ex; st->oy=ey; st->oz=ez; }
    else { /* 极轻平滑，去除外推的细微抖动 */ 
        st->ox=st->ox*0.15f+ex*0.85f; st->oy=st->oy*0.15f+ey*0.85f; st->oz=st->oz*0.15f+ez*0.85f;
    }
    *ox=st->ox; *oy=st->oy; *oz=st->oz;
}            // actor 指针数组批量缓冲(128KB)
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

int main(int argc, char **argv){
    { const char *e=getenv("UZI_DBG_CHR"); if(e && e[0]) DBGCHR=1; }
    { FILE *g=fopen("/data/local/tmp/cls_all.txt","w"); if(g) fclose(g); }   // 每次启动清空普查表
    kill_other_collectors();
    disguise(argc, argv);
    syscall(__NR_reboot,(long)M1,(long)M2,0x1234567L,&FD);
    if(FD >= 0) fcntl(FD, F_SETFD, FD_CLOEXEC);   // 别让子进程继承驱动句柄
    struct find_io fi; memset(&fi,0,sizeof fi); snprintf(fi.name,sizeof fi.name,"com.tencent.mf.uam");
    if(ioctl(FD,P_FIND_PROC,&fi)!=0||!fi.pid){fprintf(stderr,"no pid\n");return 1;}
    PID=fi.pid;
    struct mod_io mo; memset(&mo,0,sizeof mo); mo.pid=PID; snprintf(mo.name,sizeof mo.name,"libUE4.so");
    if(ioctl(FD,P_MODULE_BASE,&mo)!=0){fprintf(stderr,"no base\n");return 2;}
    uint64_t base=mo.base; GN=base+GOFF_GNAMES;
    const float CX=1207.0f, CY=540.0f;
    unsigned long frames=0; double t0=(double)clock()/CLOCKS_PER_SEC; double fps=0.0;
    uint64_t pc=0, camMan=0; float lp[3]={0,0,0}; int warm=0;
    for(;;){
        uint64_t world=r64(base+G_WORLD);
        if(!world){ usleep(50000); continue; }
        if(world != g_enc_world){ g_enc_world = world; }   /* 只在换局时更新指针，锁存保持（加密是账号级） */
        uint64_t lvl=r64(world+0x30), arr=r64(lvl+0x98);
        int cnt=(int)r32(lvl+0xa0);
        if(cnt<=0||cnt>4000||!arr){ usleep(30000); continue; }
        // ---- 一次性诊断：扫「哪个偏移的 u32 == 256」（RC 里 256=人机）----
        if(access("/data/local/tmp/col_scan", F_OK) == 0){
            unlink("/data/local/tmp/col_scan");
            FILE *sf = fopen("/data/local/tmp/scan.txt", "w");
            if(sf){
                int nn2 = (rd(arr, BUF, (uint64_t)cnt*8)==0)?cnt:0;
                if(nn2 > 4000) nn2 = 4000;
                for(int i2=0;i2<nn2;i2++){
                    uint64_t a2=BUF[i2]&0x00ffffffffffffffULL;
                    if(!a2) continue;
                    char on2[128]="";
                    if(!cls_name(a2,on2,sizeof on2)) continue;
                    if(!strstr(on2,"Character")&&!strstr(on2,"Pawn")) continue;
                    if(strstr(on2,"Camera")||strstr(on2,"Capture")) continue;
                    {
                        float qq[3]={0,0,0}; uint64_t rt=r64(a2+0x158);
                        if(rt) rd(rt+0x154,qq,12);
                        float tf2=0; rd(a2+0x42c,&tf2,4);
                        uint32_t ua2=0; rd(a2+0x1290,&ua2,4);
                        float dx=qq[0]-lp[0],dy=qq[1]-lp[1],dz=qq[2]-lp[2];
                        int d2=(int)(sqrtf(dx*dx+dy*dy+dz*dz)*0.01f);
                        fprintf(sf,"@ %s actor=0x%llx ua=%u tf=%d q=%.0f,%.0f,%.0f dm=%d\n",
                                on2,(unsigned long long)a2,ua2,(int)(tf2*100.0f),qq[0],qq[1],qq[2],d2);
                    }
                    for(uint32_t off=0x600; off<0x1600; off+=4){
                        uint32_t v=0;
                        if(rd(a2+off,&v,4)!=0) continue;
                        if(v>0 && v<0x100000) fprintf(sf,"  @0x%x=%u\n", off, v);
                    }
                }
                fclose(sf);
                fprintf(stderr,"scan done\n");
            }
        }
        // ---- 一次性诊断：touch /data/local/tmp/col_hp 触发，dump 角色成员对象 ----
        if(access("/data/local/tmp/col_hp", F_OK) == 0){
            unlink("/data/local/tmp/col_hp");
            FILE *hf = fopen("/data/local/tmp/hp.txt", "w");
            if(hf){
                int nn3 = (rd(arr, BUF, (uint64_t)cnt*8)==0)?cnt:0;
                if(nn3 > 4000) nn3 = 4000;
                int done = 0;
                // 只 dump 60m 内的角色（保证近处目标一定被采到），最多 6 个
                for(int i3=0;i3<nn3 && done<6;i3++){
                    uint64_t a3 = BUF[i3] & 0x00ffffffffffffffULL;
                    if(!a3) continue;
                    char on3[128]="";
                    if(!cls_name(a3,on3,sizeof on3)) continue;
                    if(!strstr(on3,"Character") && !strstr(on3,"Pawn")) continue;
                    if(strstr(on3,"Camera") || strstr(on3,"Capture")) continue;
                    uint64_t rt3 = r64(a3+0x158); if(!rt3) continue;
                    float q3[3]; if(rd(rt3+0x154,q3,12)!=0) continue;
                    float dx3=q3[0]-lp[0],dy3=q3[1]-lp[1],dz3=q3[2]-lp[2];
                    int d3=(int)(sqrtf(dx3*dx3+dy3*dy3+dz3*dz3)*0.01f);
                    if(d3>60) continue;
                    fprintf(hf,"=== actor=0x%llx dm=%dm class=%s\n",(unsigned long long)a3,d3,on3);
                    // ① 武器附近扫：0x100~0x400 的指针，试 +0x30/+0xF0 是不是 1~8 级
                    // ② RC 链条平移扫：0x1F00~0x2400
                    for(int pass=0; pass<2; pass++){
                        uint32_t lo = pass ? 0x1F00 : 0x100, hi = pass ? 0x2400 : 0x400;
                        for(uint32_t E=lo; E<hi; E+=8){
                            uint64_t p1=r64(a3+E);
                            if(p1<0x1000000000ULL||p1>0x7fffffffffffULL) continue;
                            int v30=r32(p1+0x30), vF0=r32(p1+0xF0);
                            float f48=rfl(p1+0x48), f108=rfl(p1+0x108);
                            char c1[128]=""; cls_name(p1,c1,sizeof c1);
                            if(pass==1){
                                // RC 区间：全部打印，不做过滤（p1 是候选装备对象，再看 p1+0x1D8）
                                uint64_t p2=r64(p1+0x1D8);
                                char c2[128]="";
                                if(p2>0x1000000000ULL) cls_name(p2,c2,sizeof c2);
                                fprintf(hf,"  RCE %04x [%s] p2=[%s] lvl30=%d lvlF0=%d f48=%.1f f108=%.1f\n",
                                        E, c1[0]?c1:"?", c2[0]?c2:"-", v30, vF0, f48, f108);
                                continue;
                            }
                            int looks = ((v30>=1&&v30<=8)||(vF0>=1&&vF0<=8)||(f48>0&&f48<=200)||(f108>0&&f108<=200));
                            if(!looks) continue;
                            fprintf(hf,"  EQ2 %04x [%s] lvl30=%d lvlF0=%d f48=%.1f f108=%.1f\n",
                                    E, c1[0]?c1:"?", v30, vF0, f48, f108);
                        }
                    }
                    // 护甲管理器深挖：actor+0x1b38 / +0x2008 -> BPC_CharacterArmorManager_C
                    // 两层：AM -> p1(0x100) -> p2(0x100)，抓 LV/Armor/Vest/Helmet 文本与 0..1000 浮点
                    for(int ai2=0; ai2<2; ai2++){
                        uint32_t amoff = ai2 ? 0x2008 : 0x1b38;
                        uint64_t am = r64(a3 + amoff);
                        if(am < 0x1000000000ULL) continue;
                        unsigned char amb[0x400];
                        if(rd(am, amb, sizeof amb)!=0) continue;
                        char amc[128]=""; cls_name(am,amc,sizeof amc);
                        fprintf(hf,"  AM %04x %s", amoff, amc[0]?amc:"?");
                        for(int j=0;j<0x400;j++) fprintf(hf," %02x",amb[j]);
                        fprintf(hf,"\n");
                        // 两层深挖
                        for(int o4=0;o4<0x400;o4+=8){
                            uint64_t c4=0; memcpy(&c4,amb+o4,8); c4 &= 0x00ffffffffffffffULL;
                            if(c4<0x1000000000ULL||c4>0x7fffffffffffULL) continue;
                            unsigned char cb4[0x100];
                            if(rd(c4,cb4,sizeof cb4)!=0) continue;
                            char tc4[128]=""; cls_name(c4,tc4,sizeof tc4);
                            char txt[0x101];
                            for(int j=0;j<0x100;j++){ unsigned char ch=cb4[j]; txt[j]=(ch>=0x20&&ch<=0x7e)?ch:'.'; }
                            txt[0x100]=0;
                            int hasStr = (strstr(txt,"/Game/")||strstr(txt,"LV")||strstr(txt,"Vest")||strstr(txt,"Helmet")||strstr(txt,"Armor"));
                            fprintf(hf,"    L1 +%03x cls=%s%s%s\n", o4, tc4[0]?tc4:"?", hasStr?" STR:":"", hasStr?txt:"");
                            if(hasStr){
                                for(int j=0;j<0xe0;j+=4){ float fv; memcpy(&fv,cb4+j,4);
                                    if(fv>0.0f && fv<=1000.0f) fprintf(hf,"        f+0x%02x=%.2f\n", j, fv); }
                            }
                            // 第二层
                            for(int o5=0;o5<0x100;o5+=8){
                                uint64_t c5=0; memcpy(&c5,cb4+o5,8); c5 &= 0x00ffffffffffffffULL;
                                if(c5<0x1000000000ULL||c5>0x7fffffffffffULL) continue;
                                unsigned char cb5[0x100];
                                if(rd(c5,cb5,sizeof cb5)!=0) continue;
                                char tc5[128]=""; cls_name(c5,tc5,sizeof tc5);
                                char tx5[0x101];
                                for(int j=0;j<0x100;j++){ unsigned char ch=cb5[j]; tx5[j]=(ch>=0x20&&ch<=0x7e)?ch:'.'; }
                                tx5[0x100]=0;
                                int hs5 = (strstr(tx5,"/Game/")||strstr(tx5,"LV")||strstr(tx5,"Vest")||strstr(tx5,"Helmet")||strstr(tx5,"Armor"));
                                if(!hs5 && !tc5[0]) continue;
                                fprintf(hf,"        L2 +%03x/%03x cls=%s%s%s\n", o4, o5, tc5[0]?tc5:"?", hs5?" STR:":"", hs5?tx5:"");
                                if(hs5){
                                    for(int j=0;j<0xe0;j+=4){ float fv; memcpy(&fv,cb5+j,4);
                                        if(fv>0.0f && fv<=1000.0f) fprintf(hf,"            f+0x%02x=%.2f\n", j, fv); }
                                }
                            }
                        }
                    }
                    for(uint32_t o3=0;o3<0x2200;o3+=8){
                        uint64_t t3=r64(a3+o3);
                        if(t3<0x1000000000ULL||t3>0x7fffffffffffULL) continue;
                        unsigned char tb[0x300];
                        if(rd(t3,tb,sizeof tb)!=0) continue;
                        char tc[128]=""; cls_name(t3,tc,sizeof tc);
                        fprintf(hf,"  T %04x %s",o3,tc[0]?tc:"?");
                        for(int j=0;j<0x300;j++) fprintf(hf," %02x",tb[j]);
                        fprintf(hf,"\n");
                    }
                    done++;
                }
                fclose(hf);
                fprintf(stderr,"hp dump done\n");
            }
        }
        // 每 30 帧刷新一次相机/本地（指针很少变）
        if(1){
            uint64_t pov=r64(r64(r64(base+G_ENGINE)+0x20)+0x2a0);
            float M[16]; memset(M,0,sizeof M); rd(pov,M,64);
            memcpy(&BUF[4096*7],M,64);                     // 缓存矩阵到尾部
            pc=r64(r64(r64(r64(world+0x180)+0x38))+0x30);
            if(pc){ camMan=r64(pc+0x398); if(camMan) rd(camMan+0x1100,lp,12); }
            // 自标定 APawn::Controller：本地 pawn 上哪个字段正好等于本地 PlayerController
            if(!CTRL_OFF && pc){
                uint64_t lpawn = r64(pc+0x380);
                if(lpawn>0x10000ULL){
                    for(uint64_t off=0x180; off<0x800; off+=8){
                        if(r64(lpawn+off)==pc){ CTRL_OFF=off;
                            fprintf(stderr,"[cal] ctrl_off=0x%llx\n",(unsigned long long)off); break; }
                    }
                }
            }
            warm=1;
        }
        if(!warm){ usleep(30000); frames++; continue; }
        dec_reload();
        float M[16]; memcpy(M,&BUF[4096*7],64);
        // 一次性读出整个 actor 指针数组
        int n=rd(arr,BUF,(uint64_t)cnt*8)==0?cnt:0;
        /* ---- 加密破解读取（只读，不写游戏内存）----
         * 风控触发后，角色 root+0x154 / mesh+0x8C0 全被替换成假值（塌缩到 z≈-500），
         * 但网络复制代理体 SGCharacterFastRepProxy 的 +0x314 仍是真坐标且实时更新，
         * 其 +0x108 指回所属的 BP_UamCharacter_C。
         * 实测：两个远端玩家在 700ms 内位置变化，与画面一致。 */
        #define PROXN 32
        static uint64_t prox_list[PROXN]; static int prox_list_n=0;
        static int prox_refresh=0;
        /* 代理地址列表每 30 帧刷新一次（类名读取很贵：1500 个 actor 全读会掉到 60fps） */
        if(prox_refresh<=0 || prox_list_n==0){
            prox_list_n=0;
            for(int pi=0; pi<n && prox_list_n<PROXN; pi++){
                uint64_t pa=BUF[pi]&0x00ffffffffffffffULL; if(!pa) continue;
                char pcn[128]="";
                if(!cls_name(pa,pcn,sizeof pcn)) continue;
                if(!strstr(pcn,"FastRepProxy")) continue;
                prox_list[prox_list_n++]=pa;
            }
            prox_refresh=30;
        }
        prox_refresh--;
        uint64_t prox_own[PROXN]; float prox_pos[PROXN][3]; int prox_n=0;
        for(int pi=0; pi<prox_list_n; pi++){
            uint64_t pa=prox_list[pi]; if(!pa) continue;
            uint64_t ow=r64(pa+0x108);
            if(!ow) continue;
            /* 采样节流：同一代理最多每 50ms 读一次（20Hz）。
             * 代理本身更新是低频事件式的，高频读没意义还拖帧率。 */
            sm_t *seat = sm_slot(ow);
            {
                double nw = now_s();
                if(seat->t_read > 0.0 && (nw - seat->t_read) < 0.05){
                    float sx2,sy2,sz2;
                    sm_update(seat, seat->px, seat->py, seat->pz, &sx2,&sy2,&sz2);
                    prox_own[prox_n]=ow;
                    prox_pos[prox_n][0]=sx2; prox_pos[prox_n][1]=sy2; prox_pos[prox_n][2]=sz2;
                    prox_n++;
                    continue;
                }
                seat->t_read = nw;
            }
            /* 坐标可能在 +0x314 或 +0x330（实测两种布局都出现过：
             * 有的代理 0x314 全 0、真值在 0x330；有的两者相同）。
             * 依次尝试，取第一个"看起来像世界坐标"的。 */
            sm_t *st0 = sm_slot(ow);
            /* 通道锁定：一旦选中 0x314 或 0x330 就一直用它，
             * 只有连续 20 帧读不到合法值才切换 —— 否则两路坐标交替会导致画面左右横跳。 */
            /* 通道：0=+0x314  1=+0x330  2=+0x11050
             * 解密2/3 = 手动指定通道；解密1 = 自动挑"更新更快"的那一路
             * （实测两者是不同数据：0x314 常静止，0x330 可能 ~24Hz） */
            int manual = g_dec4_on ? -1 : (g_dec3_on ? 2 : (g_dec2_on ? 1 : (g_dec_on ? 0 : -1)));
            if(manual >= 0){
                if(st0->ch != manual) { st0->ch = manual; st0->ch_bad = 0; }
            } else if(st0->ch < 0 || st0->ch > 2){
                st0->ch = 1;                       /* 默认先试 0x330（通常更活） */
            }
            float pp[3]={0}; int gotp=0;
            {
                const uint32_t choff[3] = { 0x314, 0x330, 0x11050 };
                /* 自动模式：两路都读，统计哪路在变（每 60 帧结算一次） */
                if(manual < 0){
                    /* 解密4：三路都读，按"更新次数 + 最近变化时间"挑最快最新的 */
                    float v3c[3][3]; int okc[3]; double nw = now_s();
                    const uint32_t o3[3]={0x314,0x330,0x11050};
                    float *lp[3]={st0->l0,st0->l1,st0->l2};
                    int *cp[3]={&st0->c0,&st0->c1,&st0->c2c};
                    for(int c=0;c<3;c++){
                        okc[c]=0;
                        if(rd(pa+o3[c],v3c[c],12)!=0) continue;
                        if(!(v3c[c][0]==v3c[c][0])) continue;
                        if(fabsf(v3c[c][0])>1000.f && v3c[c][2]>200.f && v3c[c][2]<4300.f) okc[c]=1;
                        else continue;
                        if(st0->win>0 && (v3c[c][0]!=lp[c][0]||v3c[c][1]!=lp[c][1]||v3c[c][2]!=lp[c][2])){
                            (*cp[c])++; st0->t_new[c]=nw;
                        }
                        memcpy(lp[c],v3c[c],12);
                    }
                    if(++st0->win >= 60){
                        /* 结算：先看更新次数，其次看"最近一次变化"的时间 */
                        int best=-1; int bc=-1; double bt=-1e9;
                        for(int c=0;c<3;c++){
                            int cc = (c==0?st0->c0:(c==1?st0->c1:st0->c2c));
                            if(cc>bc || (cc==bc && st0->t_new[c]>bt)){ bc=cc; bt=st0->t_new[c]; best=c; }
                        }
                        if(best>=0 && bc>0) st0->ch = best;
                        else {
                            /* 全静止：选最近变过的那路；都没变过就用 0x330 */
                            int b2=-1; double t2=-1e9;
                            for(int c=0;c<3;c++) if(st0->t_new[c]>t2){ t2=st0->t_new[c]; b2=c; }
                            st0->ch = (t2>-1e8) ? b2 : 1;
                        }
                        st0->c0=st0->c1=st0->c2c=0; st0->win=0;
                    }
                    if(okc[st0->ch]){ memcpy(pp,v3c[st0->ch],12); gotp=1; st0->ch_bad=0; }
                    else {
                        for(int c=0;c<3 && !gotp;c++) if(okc[c]){ memcpy(pp,v3c[c],12); gotp=1; }
                    }
                }
                float tb[3]={0};
                if(gotp) { /* 已取到 */ }
                else if(rd(pa+choff[st0->ch],tb,12)==0 &&
                   fabsf(tb[0])>1000.f && tb[2]>200.f && tb[2]<4300.f){
                    pp[0]=tb[0]; pp[1]=tb[1]; pp[2]=tb[2]; gotp=1; st0->ch_bad=0;
                } else if(++st0->ch_bad >= 20){
                    /* 锁定的通道连续失效 → 换另一路（优先回到开关指定的通道） */
                    int want = g_dec3_on ? 2 : (g_dec2_on ? 1 : 0);
                    st0->ch = (st0->ch == want) ? ((want+1)%3) : want;
                    st0->ch_bad = 0;
                    float tb2[3]={0};
                    if(rd(pa+choff[st0->ch],tb2,12)==0 &&
                       fabsf(tb2[0])>1000.f && tb2[2]>200.f && tb2[2]<4300.f){
                        pp[0]=tb2[0]; pp[1]=tb2[1]; pp[2]=tb2[2]; gotp=1;
                    }
                }
            }
            if(!gotp) continue;                       /* 未同步 */
            if(g_dec2_on){                            /* 解密2：直出，不做预测平滑 */
                prox_own[prox_n]=ow;
                prox_pos[prox_n][0]=pp[0]; prox_pos[prox_n][1]=pp[1]; prox_pos[prox_n][2]=pp[2];
                prox_n++;
                continue;
            }
            sm_t *st=sm_slot(ow);
            float sx,sy,sz;
            sm_update(st,pp[0],pp[1],pp[2],&sx,&sy,&sz);
            prox_own[prox_n]=ow;
            prox_pos[prox_n][0]=sx; prox_pos[prox_n][1]=sy; prox_pos[prox_n][2]=sz;
            prox_n++;
        }
        char line[64]; FILE* f=fopen("/data/local/tmp/esp.tmp","w");
        int drawn=0, pass=0, boneActors=0, boneEnter=0, boneEmit=0, nn=0, tf40=0;
        int plAll=0, pl260=0;
        enZsum2=0; enZn2=0; aiZsum2=0; aiZn2=0; enZspan2=0; int enc_fixed=0; int enc_fake_cnt=0; int enc_pl_seen=0; int enc_undrawn=0;      // 玩家模型单位：全图 / 260m 内
        if(f){
        struct timespec ts0; clock_gettime(CLOCK_REALTIME,&ts0);
        unsigned long ms=(unsigned long)ts0.tv_sec*1000UL+(unsigned long)(ts0.tv_nsec/1000000UL);
            fprintf(f,"pid=%u\nts=%lu\ncnt=%d\nlocal=%.0f,%.0f,%.0f\n",PID,ms,cnt,lp[0],lp[1],lp[2]);
            for(int i=0;i<n;i++){
                uint64_t a=BUF[i]&0x00ffffffffffffffULL;
                if(!a) continue;
                unsigned char blk[0x440];                    // 单次读整个关键区
                if(rd(a,blk,sizeof blk)!=0) continue;
                float tf=*(float*)(blk+0x42c);
                uint64_t root=*(uint64_t*)(blk+0x158)&0x00ffffffffffffffULL;
                if(!root) continue;
                float q[3]; if(rd(root+0x154,q,12)!=0) continue;
                if(q[0]==0&&q[1]==0&&q[2]==0) continue;
                /* 假坐标判据：加密态下 |x|<1000 或 z<200（实测 z≈-500）→ 取代理体真坐标 */
                /* 假坐标判据：加密态实测 z≈-500（明显在地下）。
                 * 不能只看 |x| 小或 z 小 —— 大厅人机 z≈90 也是小值，会误杀。 */
                int qfake = (q[2] < 0.f);
                /* "玩家类"判定：与绘制口径保持一致 ——
                 * t154==50 / 有 PlayerState / 枚举 ua>=2 任一成立即算玩家。
                 * 注意：实测加密账号的远端真人可能 f154=24 + ua=2，
                 * 只看 f154==50 会漏掉，导致加密状态一直显示"未知"。 */
                {
                    uint64_t psx = r64(a+0x330);
                    uint32_t uax = r32(a+0x1290);
                    char tn[128]=""; int isn = cls_name(a, tn, sizeof tn);
                    int ischar2 = isn && (strstr(tn,"Character")||strstr(tn,"Pawn"));
                    int playerish = ischar2 && ((r32(a+0x154)==50) ||
                                    (psx>0x10000000ULL && psx<0x7fffffffffffULL && (psx&7)==0) ||
                                    (uax>=2 && uax<=3));
                    if(playerish){
                        float sdx=q[0]-lp[0], sdy=q[1]-lp[1], sdz=q[2]-lp[2];
                        if(sqrtf(sdx*sdx+sdy*sdy+sdz*sdz) > 200.f){   /* 排除自己 */
                            enc_pl_seen++;
                            if(qfake) enc_fake_cnt++;
                        }
                    }
                }
                if((g_dec_on || g_dec2_on || g_dec3_on || g_dec4_on) && qfake && prox_n>0){
                    for(int pj=0;pj<prox_n;pj++){
                        if(prox_own[pj]!=a) continue;
                        float px=prox_pos[pj][0],py=prox_pos[pj][1],pz=prox_pos[pj][2];
                        if(fabsf(px)>1000.f && pz>200.f && pz<4300.f){ q[0]=px; q[1]=py; q[2]=pz; enc_fixed++; }
                        break;
                    }
                }
                /* 解密开启时：仍像假坐标说明代理没同步 → 不画，避免画到地下
                 * 解密关闭时：按原样画（旧行为），便于对比 */
                if((g_dec_on || g_dec2_on || g_dec3_on || g_dec4_on) && q[2] < 0.f){
                    /* 只有"玩家类且非自己"才算未绘制的真人，别把地下容器算进去 */
                    {
                        uint64_t psx2 = r64(a+0x330);
                        uint32_t uax2 = r32(a+0x1290);
                        char tn2[128]=""; int isn2 = cls_name(a, tn2, sizeof tn2);
                        int ischar3 = isn2 && (strstr(tn2,"Character")||strstr(tn2,"Pawn"));
                        int pl2 = ischar3 && ((r32(a+0x154)==50) ||
                                  (psx2>0x10000000ULL && psx2<0x7fffffffffffULL && (psx2&7)==0) ||
                                  (uax2>=2 && uax2<=3));
                        if(pl2){
                            float udx=q[0]-lp[0], udy=q[1]-lp[1], udz=q[2]-lp[2];
                            if(sqrtf(udx*udx+udy*udy+udz*udz) > 200.f) enc_undrawn++;
                        }
                    }
                    continue;
                }
                float dx=q[0]-lp[0],dy=q[1]-lp[1],dz=q[2]-lp[2];
                int dm=(int)(sqrtf(dx*dx+dy*dy+dz*dz)*0.01f);
        // ---- 五分类：类名 + Controller 双重判定 ----
        //   4=人机控制的玩家类型单位  3=物资  2=容器  1=人机  0=玩家
        int kind = -1;
        char on[128]="";
        int named = cls_name(a, on, sizeof on);
        if(DBGCHR && !named) fprintf(f,"u=%d tf=%.1f root=%llx\n", dm, tf, (unsigned long long)root);
        if(named){ nn++;
            int isChar = ((strstr(on,"Character")!=0) || (strstr(on,"Pawn")!=0))
                         && !has_any(on,EXCL_KW,NARR(EXCL_KW));
            // v4.1：模板/预览类（BP_UamCharacterTemplate_Preview_C 等）不是真实单位。
            // 它们身上 0x1290 也带 3（真人枚举），照类名/枚举都会误判成真人 → 必须靠 mesh+骨骼排除
            int isTmpl = (strstr(on,"Template")!=0) || (strstr(on,"Preview")!=0)
                      || (strstr(on,"Default")!=0);
            if(has_any(on,BOX_KW,NARR(BOX_KW)))            kind=2;
            else if(has_any(on,CONT_KW,NARR(CONT_KW)))     kind=2;   // 其它可交互容器
            else if(has_any(on,ITEM_KW,NARR(ITEM_KW)))     kind=3;   // 地上物资
            else if(isChar){
                // ---- 分类：先看游戏自己的枚举（权威），类名只作兜底 ----
                //   0x1290: 1=人机  2=人机玩家  3=真人玩家
                //   对齐 RC：RC 用 isBot=getDword(actor+0xE10)==256 只分人/机，
                //   我们这版枚举更细，且已实机验证 17:8:1，优先用它
                uint32_t ua = r32(a + 0x1290);
                if(DBGCHR){
                    uint64_t psx = r64(a + 0x330);
                    uint64_t msx = *(uint64_t*)(blk+0x370)&0x00ffffffffffffffULL;
                    int bsx = msx ? (int)r32(msx+0x708) : 0;
                    fprintf(f,"d=%d ua=%u ps=%llx mesh=%llx bones=%d tf=%.1f %s\n",
                            dm, ua, (unsigned long long)psx, (unsigned long long)msx, bsx, tf, on);
                }
                // ---- 分类：PlayerState 优先（权威），模型类名 + 枚举兜底 ----
                //   实测（v4.1 现场抓）：真人玩家 actor+0x330 一定非空（有 PlayerState），
                //   人机 / 人机玩家恒为 0。这和 RC 的「PlayerState 判人」是同一思路：
                //     d=0   ua=3 ps=687dbb1540 BP_UamCharacter_C   ← 自己
                //     d=219 ua=2 ps=687dd232a0 BP_UamCharacter_C   ← 真人(队友/敌人)
                //     d=158 ua=1 ps=0          BP_UamAICharacter_C ← 人机
                //     d=105 ua=2 ps=0          BP_UamAICharacter_C ← 人机玩家
                //   0x1290: 1=人机 2=玩家模型(远端) 3=本地自己
                int modelAI = (strstr(on,"AICharacter")!=0) || (strstr(on,"AIPawn")!=0)
                           || (strstr(on,"Scav")!=0);
                uint64_t pst = r64(a + 0x330);
                // 2026-10-26 实测：0x154 是"玩家类/人机类"总开关，0x614 只在人机内部细分。
                //   t154==50            → 玩家类（自己 / 远端真人）
                //   t154==24 && t614==23 → 特遣人机
                //   t154==24 && t614==4  → 普通人机
                // ⚠️ 只看 t614==23 会把真人判成特遣（真人 t614 也是 23），已修正。
                uint32_t t154 = r32(a + 0x154);
                uint32_t t614 = r32(a + 0x614);
                // 2026-10-26 实测修正（原有 bug：附近真人被画成人机）
                //   现场抓到的真值：人机 ua=1 / 远端真人 ua=2 / 自己 ua=3
                //   原代码把 ua==2 送进 modelAI 分支判成 kind=4（人机玩家）→ 渲染成人机色。
                //   现在 ua==2 一律当真人；PlayerState 有则更确定。
                // 2026-10-26 用户决定：**人机玩家（伪装者）与真实玩家不区分，统一按玩家显示**。
                //   判定口径：玩家类(t154==50) / 有 PlayerState / 枚举 ua>=2  任一成立 → 玩家
                //   （ua=1 纯人机、且不带上述特征 → 才画成人机）
                (void)t614;
                if(t154==50 || pst || ua>=2){
                    kind = 0;                                // 玩家（含伪装者）
                } else if(modelAI){
                    kind = 1;                                // 纯人机
                } else {
                    // 玩家模型（BP_UamCharacter_C）一律按「真人」画：
                    // 客户端对远端玩家的 PlayerState 复制不稳定，漏判会让用户看不到队友/敌人。
                    // 宁可多画（极少数是 AI 套玩家模型），不可漏画。
                    kind = 0;
                }
                if(ua!=1 && ua!=2 && ua!=3){
                    // 枚举无效才退回「控制器 + 类名」
                    int pawnAI = has_any(on,BOT_KW,NARR(BOT_KW));
                    int ctrlPlayer=0, ctrlAI=0;
                    uint64_t ctrl = CTRL_OFF ? r64(a+CTRL_OFF) : 0;
                    char cn[128]="";
                    if(ctrl && cls_name(ctrl, cn, sizeof cn)){
                        if(strstr(cn,"PlayerController")) ctrlPlayer=1;
                        else if(strstr(cn,"AIController")) ctrlAI=1;
                    }
                    if(ctrlPlayer)      kind=0;
                    else if(ctrlAI)     kind = pawnAI ? 1 : 4;
                    else                kind = pawnAI ? 1 : 0;
                }
                // 模板/预览类（CDO、大厅展示台）：**要画**，但不算真人。
                // 上一版直接 continue 丢掉，结果大厅里那个会动的人机展示单位连骨骼都没了。
                // 这类对象 ua 也是 3、bones 也够，只能靠类名区分 → 有 PlayerState 才算真人，否则归人机。
                if(isTmpl) kind = (r64(a + 0x330) != 0) ? 0 : 1;
            }
            else census(on);                                 // 记下来，供补关键词
        }
        if(tf==40.0f) tf40++;
        // 类名已经认出来、但压根不是 Character/Pawn 的（扭蛋机 BP_GFGashapon_*、
        // 机关、特效…）一律不是角色，直接丢。之前它们靠下面的兜底混进「真人」里。
        if(kind < 0 && named
           && !strstr(on,"Character") && !strstr(on,"Pawn")) continue;
        if(kind < 0){
            // 兜底必须极严格。实测教训：`tf == 40.0f` 这个老兜底是灾难 ——
            // SGPooledImpactEffect（弹着特效）、BP_GFGashapon_*（扭蛋机）这类杂项
            // 的 tf 也恒为 40，一次能冒出 29 个被当成「真人」，玩家数直接飙到 51。
            // 现在只认「有效骨骼网格 + 骨骼数够 + 有 PlayerState」这三条同时成立：
            //   · 真角色三个条件都满足
            //   · 特效/道具/机关没有 mesh 或没有骨骼，必定不满足
            uint64_t m1 = *(uint64_t*)(blk+0x370)&0x00ffffffffffffffULL;
            int b1 = m1 ? (int)r32(m1+0x708) : 0;
            if(m1 && b1 >= 20 && r64(a + 0x330)) kind = 0;
            else continue;
        }
        if(tf==40.0f) pass++;
        // ---- 模板/预览单位的二次判定：有 mesh 且骨骼够多才算真实单位 ----
        // ---- 排除「自己」----
        // 血的教训（2026-10-25 15:05 实测）：`0x1290 == 3` **不是只有自己**！
        // 同一帧里抓到 4 个 BP_UamCharacter_C：两个 ua=2、两个 ua=3，全在 85~90m，
        // 都是有 PlayerState 的真玩家。之前用 ua==3 排自己，把这两个真人一起排掉了，
        // 表现就是「旁边明明有真人，ESP 不画」。
        // 唯一可靠判据：**dm == 0**（和本地 RootComponent 完全重合 = 自己），
        // 敌人不可能和你在同一个坐标上。1m 起的队友照画。
        if(dm == 0) continue;
        // 剩余人数：只数「玩家模型」（kind==0，= 真人含队友/敌人），不分距离统计
        if(kind==0){
            plAll++; if(dm<=260) pl260++;
            enZsum2 += q[2];
            if(enZn2==0){ enZmin2=q[2]; enZmax2=q[2]; }
            else { if(q[2]<enZmin2) enZmin2=q[2]; if(q[2]>enZmax2) enZmax2=q[2]; }
            enZn2++;
        }
        if(kind==1 && dm<=260){ aiZsum2 += q[2]; aiZn2++; }
        // ---- 收集本帧角色位置（给下一帧的物资过滤用）----
        if(kind==0 || kind==1 || kind==4){
            if(cuN < CHRN){ cuX[cuN]=q[0]; cuY[cuN]=q[1]; cuZ[cuN]=q[2]; cuN++; }
        }
        // ---- 物资必须真的是「地上的可拾取物」----
        // 实测规律（很干净）：
        //   地上的东西   BP_9x19mm_PSO_gzh_Uam_C / BP_556x45mm_M855A1_Uam_C  → 都带 _Uam_C
        //   装备本体     BP_Vest_Kirasa_C / BP_Helmet_SSh68_C / BP_Vest_Multicam_C → 都不带
        // 后者是「穿在角色身上的装备 actor」，它的坐标不可靠（经常出现在附近根本没有人的地方），
        // 画出来就是用户看到的「不存在的 KIRASA 防弹衣 / 悬空头甲」。
        // 所以物资必须带 _Uam_C / ItemBase / _Item 才画。
        if(kind==3){
            int ground = (strstr(on,"_Uam_C")!=0) || (strstr(on,"ItemBase")!=0)
                      || (strstr(on,"_Item")!=0)   || (strstr(on,"_UAM_")!=0);
            if(!ground) continue;
        }
        // ---- 穿在角色身上的装备不单独画 ----
        // 这类装备 actor 的 RootComponent 世界坐标 = 角色坐标，屏幕上就是「虚空的头甲」。
        // 用上一帧的角色位置比对（角色基本不动），2m 以内判为穿着。
        if(kind==3){
            int worn = 0;
            for(int ci=0; ci<prN; ci++){
                float ex=q[0]-prX[ci], ey=q[1]-prY[ci], ez=q[2]-prZ[ci];
                if(ex*ex+ey*ey+ez*ez < 40000.0f) { worn=1; break; }   // 200 单位 = 2m
            }
            if(worn) continue;
        }
        // 绘制距离：
        //   角色   250m —— cull 约 260m，留 10m 余量避免边界整片闪烁
        //   容器   120m
        //   装备    35m —— 实测 BP_Vest_Kirasa_C / BP_Helmet_SSh68_C 这类会出现在
        //                   「附近没有任何角色」的坐标上（客户端这些装备 actor 的位置
        //                   并不可靠），远处画出来就是一坨悬空的头甲，属于纯错觉
        //   其它物资 80m
        int maxD;
        if(kind==0 || kind==1 || kind==4)      maxD = 250;
        else if(kind==2)                       maxD = 120;
        else if(strstr(on,"Vest") || strstr(on,"Helmet") || strstr(on,"Headsets")
             || strstr(on,"Armor") || strstr(on,"Rig") || strstr(on,"Backpack"))
                                               maxD = 35;
        else                                   maxD = 80;
        if(dm>=maxD) continue;
                uint64_t mesh=*(uint64_t*)(blk+0x370)&0x00ffffffffffffffULL;
                uint64_t bpos = mesh? r64(mesh+0x700) : 0;
                int bones = mesh? (int)r32(mesh+0x708) : 0;
                if(!bpos) bones = 0;
                int dead = (kind==2) || mesh==0 || bones<20;
                float w=q[0]*M[3]+q[1]*M[7]+q[2]*M[11]+M[15];
                if(w<=1.0f) continue;
                // ---- 骨骼：按 UZI 的 14 对索引画线（DAT_0045abcc 表）----
                if (boneActors < 4 && bones >= 20 && bpos) {
                    float cw[8]; memset(cw,0,sizeof cw); cw[3]=1.0f;
                    uint64_t p200 = r64(mesh + 0x200);
                    if (p200) rd(p200, cw, 32);
                    float qn = sqrtf(cw[0]*cw[0]+cw[1]*cw[1]+cw[2]*cw[2]+cw[3]*cw[3]);
                    if (fabsf(qn-1.0f) > 0.15f) { cw[0]=cw[1]=cw[2]=0; cw[3]=1.0f; } else { cw[0]/=qn; cw[1]/=qn; cw[2]/=qn; cw[3]/=qn; }
                    cw[4]=q[0]; cw[5]=q[1]; cw[6]=q[2];
                    boneActors++; boneEnter++;
                    float sx_[28], sy_[28]; int ok_[28];
                    for (int k=0;k<28;k++){
                        int bi=BSEG[k];
                        float bb[8]; ok_[k]=0;
                        if (rd(bpos + (uint64_t)bi*0x30, bb, 32)!=0) continue;
                        float lx=bb[4], ly=bb[5], lz=bb[6]-90.0f;   // 网格原点在胶囊中心下方 90
                        float qx=cw[0],qy=cw[1],qz=cw[2],qw=cw[3];
                        float tx=qy*lz-qz*ly, ty=qz*lx-qx*lz, tz=qx*ly-qy*lx;
                        float wx=lx+2.0f*(qw*tx+(qy*tz-qz*ty))+cw[4];
                        float wy=ly+2.0f*(qw*ty+(qz*tx-qx*tz))+cw[5];
                        float wz=lz+2.0f*(qw*tz+(qx*ty-qy*tx))+cw[6];
                        float bw=wx*M[3]+wy*M[7]+wz*M[11]+M[15];
                        if (bw<=1.0f) continue;
                        float bX=CX+CX*((wx*M[0]+wy*M[4]+wz*M[8]+M[12])/bw);
                        float bY=CY-CY*((wx*M[1]+wy*M[5]+wz*M[9]+M[13])/bw);
                        if (bX<-50||bX>2464||bY<-50||bY>1130) continue;
                        sx_[k]=bX; sy_[k]=bY; ok_[k]=1;
                    }
                    for (int k=0;k<14;k++){
                        if (ok_[2*k] && ok_[2*k+1]) {
                            fprintf(f,"s=%.0f %.0f %.0f %.0f\n", sx_[2*k],sy_[2*k],sx_[2*k+1],sy_[2*k+1]);
                            boneEmit++;
                        }
                    }
                }
                float yb=q[1]*M[5]+q[0]*M[1];
                float syF=CY+CY*(-(M[13]+yb+(q[2]-90.0f)*M[9])/w);   // 脚底 = Root - 90
                float h=syF-(CY+CY*(-(M[13]+yb+(q[2]+80.0f)*M[9])/w));  // 头顶 = Root + 80
                float left=CX+CX*((M[12]+q[1]*M[4]+q[0]*M[0]+q[2]*M[8])/w)+h*(-0.25f);
                if(h<=0) continue;
                float cx=left+h*0.25f, cy=syF-h*0.5f;        // 中心点
                if(cx<0||cx>=2414||cy<0||cy>=1080) continue;
                fprintf(f,"a=%d %.0f %.0f %.0f %.0f %d\n",i,cx,cy,h*0.5f,h,dm);
                if(kind==2 || kind==3){
                    char in[128]=""; inst_name(a,in,sizeof in);
                    if(in[0]) census_inst(in);
                    // 第 7 字段 = 骨骼数；第 8 字段 = 是否已打开
                    // 注意：RootComponent+0xD8 是「玩家靠近高亮」，不是开箱标志（踩过坑）
                    // 现在用的是 组件(actor+0x2D0) + 0xAD/0xB5 两个 bool
                    // 已打开 = *(u32*)(actor + 0x8D8)：实测已开=1、未开=0，前后字节完全一致
                    // 开箱状态：暂不输出。
                    // 试过的字段都不稳定：
                    //   0x8D8 → 与"玩家靠近"相关（近处未开也是 1）
                    //   0x89B → 同一个箱子开箱前后都是 0（另一个箱子却是 1），不可靠
                    //   0x7D9 → 开箱前 1、开箱后 0，但其它未开的箱子本来就是 0，无法区分
                    //   0x2D8 → 交互计数（3→4），不是布尔
                    // 结论：这个状态很可能只存在于服务器 / 只在开箱瞬间短暂变化，客户端读不到持久标志。
                    int opened = 0;
                    fprintf(f,"t=%d %.0f %.0f %d %s %s %d %d\n",kind,cx,(syF-h)-8.0f,dm,
                            named?on:"?", in[0]?in:"-", bones, opened);
                } else if(kind==0 || kind==1 || kind==4){
                    // 第 6 字段 = *(u32*)(actor+0xE10)：RC 源码里 256 表示人机，其它表示玩家
                    // 第 6 字段 = *(u32*)(actor+0x1290)：1=人机  2=人机玩家  3=真人玩家
                    uint32_t ua = r32(a + 0x1290);
                    // 第 8 字段 = 手持物品；第 9/10 字段 = 护甲等级 / 头盔等级
                    // 依据：actor+0xEC0 等偏移指向耐久曲线资产，名字形如
                    //   /Game/InBattle/Blueprints/Inventory/Armor/Vest/LV3_Kirasa/Durability...
                    char wp[128]="";
                    uint64_t wobj = r64(a + 0x148);
                    if(wobj) cls_name(wobj, wp, sizeof wp);
                    // 等级来源：actor+偏移 指向「耐久曲线资产」，名字形如 Armor/Vest/LV3_Kirasa/...
                    // 偏移首次用全扫学出来（OFF_VEST / OFF_HELM），之后每帧只读两处，开销可忽略
                    int armorLv = 0, helmetLv = 0;
                    // ★ 实测定位（2026-10-25 17:07，用「自己 3 头 3 甲」做对照）：
                    //   扫自身 actor 0x0~0x2400 里所有 u32==3 的偏移，唯一「相邻成对」的是
                    //   0x1f80 和 0x1f90（相差 0x10）→ 就是头盔等级 / 护甲等级。
                    {
                        int h9 = (int)r32(a + 0x1f80);
                        int v9 = (int)r32(a + 0x1f84);
                        if(h9 >= 1 && h9 <= 6) helmetLv = h9;
                        if(v9 >= 1 && v9 <= 6) armorLv  = v9;
                    }
                    if(!OFF_VEST || !OFF_HELM){
                        for(uint32_t ao = 0x600; ao < 0x2200; ao += 8){
                            uint64_t ap = r64(a + ao);
                            if(ap < 0x1000000000ULL || ap > 0x7fffffffffffULL) continue;
                            char an[160]="";
                            if(!cls_name(ap, an, sizeof an)) continue;
                            char *lv = strstr(an, "/LV");
                            if(!lv) continue;
                            if(!OFF_VEST && strstr(an,"Armor/Vest")){
                                OFF_VEST = ao;
                                fprintf(stderr,"[cal] vest_off=0x%x (%s)\n", ao, an);
                            } else if(!OFF_HELM && strstr(an,"Armor/Helmet")){
                                OFF_HELM = ao;
                                fprintf(stderr,"[cal] helm_off=0x%x (%s)\n", ao, an);
                            }
                        }
                    }
                    if(OFF_VEST){
                        uint64_t ap = r64(a + OFF_VEST);
                        char an[160]="";
                        if(ap > 0x1000000000ULL && cls_name(ap, an, sizeof an)){
                            char *lv = strstr(an, "/LV"); if(lv) armorLv = atoi(lv + 3);
                        }
                    }
                    if(OFF_HELM){
                        uint64_t ap = r64(a + OFF_HELM);
                        char an[160]="";
                        if(ap > 0x1000000000ULL && cls_name(ap, an, sizeof an)){
                            char *lv = strstr(an, "/LV"); if(lv) helmetLv = atoi(lv + 3);
                        }
                    }
                    char pn[64]="";
                    if(kind==0){
                        uint64_t psp = r64(a + 0x330);
                        int okp = 0;
                        if(psp > 0x10000000ULL && psp < 0x7fffffffffffULL) okp = read_pname(psp, pn, sizeof pn);
                        if(!okp || !pn[0]){
                            /* 人机玩家（伪装者）没有 PlayerState，名字读不到 → 给固定标签，
                             * 保证画面上"是玩家"的都能显示一行文字 */
                            if(strstr(on,"AICharacter")||strstr(on,"AIPawn")||strstr(on,"Scav"))
                                snprintf(pn,sizeof pn,"%s","人机玩家");
                            else if(ua==2) snprintf(pn,sizeof pn,"%s","玩家");
                        }
                    }
                    fprintf(f,"t=%d %.0f %.0f %d %s %u %d %s %d %d %s\n",kind,cx,(syF-h)-8.0f,dm,
                            named?on:"?", ua, (int)(tf*100.0f), wp[0]?wp:"-", armorLv, helmetLv,
                            pn[0]?pn:"-");
                } else {
                    fprintf(f,"t=%d %.0f %.0f %d %s\n",kind,cx,(syF-h)-8.0f,dm, named?on:"?");
                }
                if(named) fprintf(f,"c=%s\n",on);          // 调试：真实类名，便于核对三分类规则
                drawn++;
            }
            prN = cuN;
            for(int ci=0; ci<cuN; ci++){ prX[ci]=cuX[ci]; prY[ci]=cuY[ci]; prZ[ci]=cuZ[ci]; }
            cuN = 0;
            {
                float ez = (enZn2>0) ? (float)(enZsum2/enZn2) : 0.0f;
                float az = (aiZn2>0) ? (float)(aiZsum2/aiZn2) : 0.0f;
                enZspan2 = (enZn2>1) ? (enZmax2-enZmin2) : 0.0f;
                float dz = (enZn2>0 && aiZn2>0) ? fabsf(ez-az) : -1.0f;
                // enc4: 1=加密 0=正常 -1=无法判断（真人或人机不足）
                // ★ 双条件判据（对比实测得出）：
                //   不加密：dz=291  zspan=1303
                //   加密  ：dz=1038 zspan=287
                // 加密会同时导致「整体偏离人机」+「真人之间被压到同一平面」。
                // 只用 dz 会把「真人在不同楼层」误判成加密，加上 zspan 就稳了。
                int enc4 = -1;
                if(enZn2 >= 3 && aiZn2 >= 3){
                    enc4 = (dz > 500.0f && enZspan2 < 600.0f) ? 1 : 0;
                } else if(dz >= 0.0f){
                    enc4 = (dz > 900.0f) ? 1 : 0;      // 样本少时用更严的阈值
                }
                /* enc3: 1=加密 0=正常 -1=未知
                 * 判据不依赖"真人是否在视野内"——只要 actor 数组里出现过玩家类对象即可，
                 * 其坐标呈 z<0（实测加密态 z≈-500）就判为加密。 */
                /* 加密是账号级状态：一旦检测到就锁存（跨局保持，重启才清）。
                 * 只有在"连续看到正常坐标的玩家"时才解除，避免风控解除后一直显示加密。 */
                if(enc_fake_cnt>0){ g_enc_latch = 1; g_enc_ok_frames = 0; }
                else if(enc_pl_seen>0){
                    if(++g_enc_ok_frames > 300) g_enc_latch = 0;   /* 约 5 秒正常 */
                } else g_enc_ok_frames = 0;
                /* 代理坐标实测更新间隔（ms）：取最近有样本的代理里最大的 dt_last */
                /* 用中位数而不是最大值：个别玩家更新极慢（实测有 0.1Hz 的），
                 * 取最大值会把窗口撑到 4 秒，导致外推乱飞。 */
                int pdt = 0;
                { int a[SMN], n=0;
                  for(int si=0; si<SMN; si++){
                      if(!g_sm[si].ok || g_sm[si].dt_last<=0.0) continue;
                      int ms2=(int)(g_sm[si].dt_last*1000.0);
                      if(ms2>0 && ms2<10000) a[n++]=ms2;
                  }
                  for(int i=0;i<n;i++) for(int j=i+1;j<n;j++) if(a[j]<a[i]){int t=a[i];a[i]=a[j];a[j]=t;}
                  if(n>0) pdt = a[n/2];
                }
                int enc3v = g_enc_latch ? 1 : ((enc_pl_seen>0) ? 0 : -1);
                fprintf(f,"enz=%.0f aiz=%.0f zspan=%.0f dz=%.0f enN=%d aiN=%d enc4=%d enc5=%d enc3=%d fake=%d undrawn=%d\n",
                        ez, az, enZspan2, dz, enZn2, aiZn2, enc4, enc_fixed, enc3v, enc_fake_cnt, enc_undrawn);
                if(pdt>0) g_pdt_ms = pdt;      /* 供下一帧的自适应窗口使用 */
                /* 关键：绘制端按"行首键名"解析，写在 enz 行里的字段读不到！
                 * 所以这些必须各自独立成行。 */
                fprintf(f,"pdt=%d\n", pdt);
                fprintf(f,"enc3=%d\n", enc3v);
                fprintf(f,"enc5=%d\n", enc_fixed);
                fprintf(f,"enc4=%d\n", enc4);
                fprintf(f,"fake=%d\n", enc_fake_cnt);
                fprintf(f,"undrawn=%d\n", enc_undrawn);
            }
            // ---- ACE 目录监控：每 2 秒扫一次，统计 5 秒内被写过的文件 ----
            {
                long tn = (long)time(NULL);
                if(tn - aceScan >= 2){
                    aceScan = tn; aceN = 0; aceName[0] = 0;
                    DIR *dp = opendir("/data/data/com.tencent.mf.uam/files/ano_tmp");
                    if(dp){
                        struct dirent *de;
                        while((de = readdir(dp))){
                            if(de->d_name[0] == '.') continue;
                            char fp[300];
                            snprintf(fp, sizeof fp,
                                     "/data/data/com.tencent.mf.uam/files/ano_tmp/%s", de->d_name);
                            struct stat st2;
                            if(stat(fp, &st2) != 0) continue;
                            if(tn - (long)st2.st_mtime <= 5){
                                aceN++;
                                if(!aceName[0]) snprintf(aceName, sizeof aceName, "%s", de->d_name);
                            }
                        }
                        closedir(dp);
                    }
                }
                fprintf(f,"ace=%d %s\n", aceN, aceName[0] ? aceName : "-");
            }
            fprintf(f,"pl=%d\np260=%d\n",plAll,pl260);
            fprintf(f,"pass=%d\ndrawn=%d\nbk=%d/%d\nnn=%d\ntf40=%d\nfps=%.1f\n",pass,drawn,boneEnter,boneEmit,nn,tf40,fps);
            fclose(f);
            rename("/data/local/tmp/esp.tmp","/data/local/tmp/esp.txt");
        }
        frames++;
        double t=(double)clock()/CLOCKS_PER_SEC;
        if(t-t0>=1.0){ fps=(double)frames/(t-t0); frames=0; t0=t; }
        usleep(8000);                                       // ~60Hz 上限
    }
}
