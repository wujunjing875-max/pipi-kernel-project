import java.lang.reflect.*;
import java.io.*;

public class Ov50 {
    // ---- 5x7 点阵字 ----
    static final String[] CH = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.:/[]="};
    static final int[][][] G = {
        {{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, //A
        {{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}}, //B
        {{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, //C
        {{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}}, //D
        {{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, //E
        {{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}}, //F
        {{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, //G
        {{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}}, //H
        {{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, //I
        {{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}}, //J
        {{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, //K
        {{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}}, //L
        {{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, //M
        {{0x11,0x19,0x15,0x13,0x11,0x11,0x11}}, //N
        {{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, //O
        {{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}}, //P
        {{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, //Q
        {{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}}, //R
        {{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, //S
        {{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}}, //T
        {{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, //U
        {{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}}, //V
        {{0x11,0x11,0x11,0x15,0x15,0x1B,0x11}}, //W
        {{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}}, //X
        {{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}}, //Y
        {{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}}, //Z
        {{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, //0
        {{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}}, //1
        {{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, //2
        {{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}}, //3
        {{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, //4
        {{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}}, //5
        {{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, //6
        {{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}}, //7
        {{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, //8
        {{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}}, //9
        {{0,0,0,0,0,0,0}},                        //space
        {{0,0,0,0x1F,0,0,0}},                     //-
        {{0,0,0,0,0,0x0C,0x0C}},                  //.
        {{0,0x0C,0x0C,0,0x0C,0x0C,0}},            //:
        {{0x01,0x02,0x04,0x08,0x10,0,0}},         ///
        {{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}},   //[
        {{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},   //]
        {{0,0x1F,0,0x1F,0,0,0}}                   //=
    };

    static Object sc, surface, paint;
    static Method mLock, mUnlock, mDrawColor, mDrawRect;
    static Method mSetColor;
    static int W, H, PX, PY;
    static final int PW = 1440;
    static volatile int PH = 470;             // 面板高度随「类型展开」动态变化
    static volatile boolean hidden = false, lastHidden = false;
    static Object clearMode = null; static java.lang.reflect.Method mDrawColorMode = null;
    static long lastSeq = -1, lastSeqMs = 0;
    static volatile boolean featA = false, featB = true;
    static volatile int touchX = -1, touchY = -1;
    static volatile long tapCount = 0;
    static volatile boolean dragging = false, needMove = false;
    static volatile int dragOffX = 0, dragOffY = 0, newPX = 0, newPY = 0;
    static volatile String espPid = "-", espBase = "-", espHdr = "-", espState = "-";
    static volatile String espReads = "-", espLoops = "-";
    static volatile int boxN = 0, txtN = 0, boneN = 0;
    static volatile float[] bnX = new float[256], bnY = new float[256];
    static volatile int segN = 0;
    static volatile float[] sgX1 = new float[128], sgY1 = new float[128], sgX2 = new float[128], sgY2 = new float[128];
    static volatile float[] txtX = new float[128], txtY = new float[128];
    static volatile int[] txtBot = new int[128], txtDm = new int[128];
    static volatile String[] txtName = new String[128];      // 类名（物资用来查价格/显示）
    static volatile String[] txtInst = new String[128];      // 实例名（通用 BP_ItemBase_C 时靠它认物）
    static volatile int[] txtOpen = new int[128];            // 容器是否已打开（1=开过）
    static volatile int[] txtBotF = new int[128];            // 角色 +0x1290：1/2/3
    static volatile String[] txtWeap = new String[128];      // 角色手持武器类名
    static volatile int[] txtArmLv = new int[128];           // 护甲等级（采集端从耐久曲线资产名解析）
    static volatile int[] txtHelLv = new int[128];           // 头盔等级
    static volatile String[] txtPName = new String[128];     // 真人玩家名字（PlayerState+0x470）
    static int[] boxKind = new int[128];
    static int[] boxType = new int[128];       // 该框对应的容器类别下标（-1=非容器）
    static int[] txtType = new int[128];

    // ---- 容器：按「类别」开关，类别 = 中文图集 key ----
    // 好处：类别是预先固定的，所以面板里永远能列出全部类型，
    // 玩家进对局前就能勾好，不必等采集端先发现某个箱子。
    static volatile boolean contMaster = true, contExpand = true;
    static final String[] CAT_KEY = {
        "n_box_player", "n_box_bot", "n_safebox", "n_med", "n_ammo", "n_weapon", "n_tool",
        "n_bag", "n_drawer", "n_wardrobe", "n_jacket", "n_pc", "n_file", "n_cash",
        "n_fridge", "n_washer", "n_case", "n_crate", "n_airdrop", "n_locker",
        "n_jewelry", "n_unknown"
    };
    // 颜色一律挑亮的：深色/灰在游戏画面上根本看不清
    static final int[] CAT_COL = {
        0xFF5FE1FF,  // 玩家盒  亮青
        0xFFB6FF3C,  // 人机盒  亮黄绿
        0xFFFFD24A,  // 保险箱  金
        0xFF4DE0C0,  // 医疗箱  青绿
        0xFFFF9A3C,  // 弹药箱  橙
        0xFFFF6B6B,  // 武器箱  红
        0xFFC0A6FF,  // 工具箱  淡紫
        0xFFFF8FD8,  // 背包    粉
        0xFFFFE066,  // 抽屉    黄
        0xFFE0B3FF,  // 衣柜    淡紫
        0xFFA8D8FF,  // 衣物    淡蓝
        0xFF63D3FF,  // 电脑机箱 天蓝
        0xFFD9D9A0,  // 文件柜  米黄
        0xFFFFC46B,  // 收银机  杏
        0xFF7FE3D9,  // 冰箱    薄荷
        0xFF8AD8FF,  // 洗衣机  蓝
        0xFFEFA26B,  // 手提箱  土黄
        0xFFFFF3B0,  // 物资箱  浅金
        0xFFFF5FA2,  // 空投箱  玫红
        0xFFB9CE6E,  // 柜子    橄榄
        0xFFF0C46B,  // 珠宝箱  金橙
        0xFFC77DFF   // 未知容器 亮紫
    };
    // 默认只开这几类（玩家盒子 / 人机盒子 / 保险箱）；未知容器默认关
    static final String[] CAT_ON_DEFAULT = { "n_box_player", "n_box_bot", "n_safebox" };
    static final int CAT_N = CAT_KEY.length;
    static final int[] catOn = new int[CAT_N];    // 1=绘制
    static final int[] catNum = new int[CAT_N];   // 本帧命中数（面板显示用）
    static int catOf(String raw) {
        String k = zhKey(raw);
        if (k == null) return CAT_N - 1;
        for (int i = 0; i < CAT_N; i++) if (CAT_KEY[i].equals(k)) return i;
        return CAT_N - 1;
    }
    static int[] txtKind = new int[128]; static volatile float[] boxX = new float[128]; static volatile float[] boxY = new float[128];
    static volatile float[] boxW = new float[128]; static volatile float[] boxH = new float[128];
    static volatile long[] mv = new long[8];
    static volatile String[] ms = new String[8];

    static Object tfCN; static Method mSetTF, mSetTS, mDrawT, mSetAA;
    static Method mSave, mRestore, mTranslate, mClip;
    // ---- 滚动 ----
    static final int TITLE_H = 60;
    static final int[] FPS_OPTS = { 30, 60, 90, 120, 144 };
    static volatile int fpsSel = 60;
    static volatile int scrollY = 0, maxScroll = 0;
    static volatile boolean scrolling = false, tapMoved = false;
    static volatile int scrollAnchorY = 0, scrollStartVal = 0;
    static volatile boolean gestureDone = false;
    static volatile boolean userMoved = false;   // 用户拖过面板后不再自动居中
    // 点击位置洋红十字（标定/调试用），默认关闭
    static final boolean SHOW_TAP_CROSS = false;
    // ---- 中文图集（PC 端预渲染，运行时贴图）----
    static Object uiBmp = null;
    static final java.util.HashMap<String,int[]> uiR = new java.util.HashMap<>();
    static Method mDrawBmp; static java.lang.reflect.Constructor<?> cRect;
    // 图集是白色字，靠 PorterDuffColorFilter(SRC_IN) 上色，否则所有标签都是白的
    static Method mSetCF; static Object cfSrcIn = null;
    static java.lang.reflect.Constructor<?> cCF;
    static final java.util.HashMap<Integer,Object> cfCache = new java.util.HashMap<>();
    static Object cfOf(int color) {
        if (cCF == null || cfSrcIn == null) return null;
        Object cf = cfCache.get(color);
        if (cf == null) {
            try { cf = cCF.newInstance(color, cfSrcIn); cfCache.put(color, cf); } catch (Throwable t) { return null; }
        }
        return cf;
    }
    static void label(Object c, String key, int x, int y) { label(c, key, x, y, 0xFFFFFFFF); }
    static void label(Object c, String key, int x, int y, int color) {
        int[] r = uiR.get(key);
        if (r == null || uiBmp == null || mDrawBmp == null) return;
        try {
            Object sr = cRect.newInstance(r[0], r[1], r[0] + r[2], r[1] + r[3]);
            Object dr = cRect.newInstance(x, y, x + r[2], y + r[3]);
            mSetColor.invoke(paint, color);
            mSetAA.invoke(paint, true);
            Object cf = cfOf(color);
            if (cf != null && mSetCF != null) mSetCF.invoke(paint, cf);
            mDrawBmp.invoke(c, uiBmp, sr, dr, paint);
            if (mSetCF != null) mSetCF.invoke(paint, new Object[]{null});
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }
    // ImGui 默认暗色主题（ImGui::StyleColorsDark）
    static final int C_WIN      = 0xF20F0F0F;   // WindowBg   0.06,0.06,0.06,0.94
    static final int C_TITLE    = 0xFF0A0A0A;   // TitleBg    0.04
    static final int C_BORDER   = 0xFF6E6E6E;   // Border     0.43
    static final int C_TEXT     = 0xFFF0F0F0;   // Text
    static final int C_DIM      = 0xFF8C8C8C;   // TextDisabled
    static final int C_FRAME    = 0xFF3B4A63;   // FrameBg    0.16,0.29,0.48
    static final int C_CHECK    = 0xFF4296FA;   // CheckMark  0.26,0.59,0.98
    static final int C_BTN      = 0xFF2A4A7C;   // Button
    static final int C_BTNHOV   = 0xFF4296FA;   // ButtonHovered
    static final int C_SEP      = 0x3CFFFFFF;

    static void checkbox(Object c, String key, boolean on, int x, int y, int rowW, int id) throws Exception {
        label(c, key, x, y + 3, on ? C_TEXT : C_DIM);
        int bx = x + rowW - 30, by = y + 6;
        rect(c, bx, by, bx + 26, by + 26, on ? C_CHECK : C_FRAME);
        rect(c, bx, by, bx + 26, by + 2, C_BORDER);
        rect(c, bx, by + 24, bx + 26, by + 26, C_BORDER);
        rect(c, bx, by, bx + 2, by + 26, C_BORDER);
        rect(c, bx + 24, by, bx + 26, by + 26, C_BORDER);
        hit(x - 6, y, rowW + 6, 40, id);
    }

    static void drawPill(Object c, String label, boolean on, int x, int y, int w, int h, int id) throws Exception {
        int bg = on ? 0xFF0E3527 : 0xFF1B1F24;
        int fg = on ? 0xFF25E6A0 : 0xFF6B757E;
        rect(c, x, y, x + w, y + h, bg);
        rect(c, x, y, x + w, y + 2, fg);
        textCN(c, label, x + 12, y + h / 2 + 7, 19, on ? 0xFFE6FFF5 : 0xFF8A939B);
        textCN(c, on ? "开" : "关", x + w - 34, y + h / 2 + 7, 17, fg);
        hit(x, y, w, h, id);
    }

    static void textCN(Object c, String s, float x, float y, float size, int color) {
        try {
            mSetColor.invoke(paint, color);
            mSetTS.invoke(paint, size);
            mSetTF.invoke(paint, tfCN);
            mSetAA.invoke(paint, true);
            mDrawT.invoke(c, s, x, y, paint);
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }

    static void rect(Object c, float l, float t, float r, float b, int color) throws Exception {
        mSetColor.invoke(paint, color);
        mDrawRect.invoke(c, l, t, r, b, paint);
    }
    static void text(Object c, String s, float x, float y, int sc2, int color) throws Exception {
        mSetColor.invoke(paint, color);
        float px = sc2, py = sc2;
        for (int i = 0; i < s.length(); i++) {
            char ch = Character.toUpperCase(s.charAt(i));
            int idx = CH[0].indexOf(ch); if (idx < 0) idx = CH[0].indexOf(' ');
            int[][] g = G[idx];
            for (int row = 0; row < 7; row++) {
                int bits = g[0][row];
                for (int col = 0; col < 5; col++) {
                    if ((bits & (1 << (4 - col))) != 0)
                        mDrawRect.invoke(c, x + col*px, y + row*py, x + (col+1)*px, y + (row+1)*py, paint);
                }
            }
            x += 6*px;
        }
    }

    static void inputThread() {
        try {
            File dir = new File("/dev/input");
            FilenameFilter f = (d, n) -> n.startsWith("event");
            String[] files = dir.list(f);
            System.out.println("[i] /dev/input 可列=" + (files != null) + " 数量=" + (files==null?0:files.length));
            if (files == null) return;
            for (final String name : files) {
                new Thread(() -> {
                    try {
                        RandomAccessFile raf = new RandomAccessFile("/dev/input/" + name, "r");
                        byte[] buf = new byte[24];
                        System.out.println("[i] 已打开 /dev/input/" + name);
                        int ax = -1, ay = -1, down = 0, tapped = 0;
                        boolean fresh = false;
                        while (true) {
                            if (raf.read(buf) < 24) continue;
                            int type = (buf[16] & 0xff) | ((buf[17] & 0xff) << 8);
                            int code = (buf[18] & 0xff) | ((buf[19] & 0xff) << 8);
                            int value = (buf[20] & 0xff) | ((buf[21] & 0xff) << 8) | ((buf[22] & 0xff) << 16) | ((buf[23] & 0xff) << 24);
                            if (type == 3 && (code == 53 || code == 54)) {
                                if (code == 53) ax = value; else ay = value;
                                fresh = true;
                            }
                            else if (type == 1 && (code == 114 || code == 115)) {
                                if (value == 1) { hidden = !hidden; }
                            }
                            else if (type == 1 && code == 330) {
                                if (value == 1) { down = 1; tapped = 0; gestureDone = false; fresh = false; }
                                else { down = 0; dragging = false; scrolling = false;
                                       if (tapped == 0) { tapped = 1; onTap(ax, ay); } }
                            }
                            else if (type == 3 && code == 57) {
                                if (value >= 0) { down = 1; tapped = 0; gestureDone = false; fresh = false; }
                                else { if (down == 1 && tapped == 0) { tapped = 1; onTap(ax, ay); }
                                       down = 0; dragging = false; scrolling = false; }
                            }
                            touchX = ax; touchY = ay;
                            // 这一笔是「拖面板」还是「滚内容」——拿到本笔第一个真实坐标时判定
                            if (down == 1 && !gestureDone && fresh && ax >= 0 && ay >= 0) {
                                gestureDone = true;
                                beginGesture(ax, ay);
                            }
                            if (down == 1 && gestureDone && (scrolling || dragging) && ax >= 0 && ay >= 0) {
                                int[] sp2 = toScreen(ax, ay);
                                int sx = sp2[0], sy = sp2[1];
                                if (scrolling) {
                                    int dy = sy - scrollAnchorY;
                                    if (Math.abs(dy) > 8) tapMoved = true;
                                    int nv = scrollStartVal - dy;
                                    if (nv < 0) nv = 0;
                                    if (nv > maxScroll) nv = maxScroll;
                                    scrollY = nv;
                                } else {
                                    int nx = sx - dragOffX, ny = sy - dragOffY;
                                    // 钳制：至少留 160px 在屏幕内，永远不会飞出去
                                    if (nx >  W - 160) nx = W - 160;
                                    if (ny >  H - 80)  ny = H - 80;
                                    if (nx < -PW + 160) nx = -PW + 160;
                                    if (ny < -PH + 80)  ny = -PH + 80;
                                    newPX = nx; newPY = ny;
                                    needMove = true;
                                }
                            }
                        }
                    } catch (Throwable t) { System.out.println("[!] input " + name + " : " + t); }
                }).start();
            }
        } catch (Throwable t) {}
    }
    // 只有按住标题栏（y 0..46）且横向移动超过阈值才进入拖拽，避免单击就把层推跑
    static volatile int dragStartX = 0, dragStartY = 0;
    // 判定这一笔是「拖面板」还是「滚内容」——在手指按下的第一个坐标事件时决定
    static void beginGesture(int rx, int ry) {
        int[] sp = toScreen(rx, ry);
        int sx = sp[0], sy = sp[1];
        float lx = sx - PX, ly = sy - PY;
        scrolling = false; dragging = false;
        if (calStep < 2) return;
        if (ly > 0 && ly < TITLE_H && lx > 0 && lx < PW) {
            dragging = true; scrollAnchorY = 0; userMoved = true;
            dragStartX = sx; dragStartY = sy;
            dragOffX = sx - PX; dragOffY = sy - PY;
            System.out.println("[GEST] drag local=" + (int)lx + "," + (int)ly);
        } else if (ly > TITLE_H && ly < PH && lx > 0 && lx < PW) {
            scrolling = true; tapMoved = false;
            scrollAnchorY = sy; scrollStartVal = scrollY;
            System.out.println("[GEST] scroll local=" + (int)lx + "," + (int)ly + " maxScroll=" + maxScroll);
        } else {
            System.out.println("[GEST] none local=" + (int)lx + "," + (int)ly + " PH=" + PH);
        }
    }

    // ---- 容器/物资 keep-alive：客户端 streaming 会让远处物件帧间消失 → 整片闪烁 ----
    // 做法：把本轮出现的容器/物资按「类型+实例名+位置格」登记，下一轮若没出现但未超过
    // KEEP_MS，就用上一帧的坐标补画一次，视觉上就稳住了。
    static final int KEEP_MS = 250;
    static int pvN = 0;
    static String[] pvKey = new String[256];
    static String[] pvName = new String[256], pvInst = new String[256];
    static float[] pvX = new float[256], pvY = new float[256];
    static int[] pvKind = new int[256], pvType = new int[256], pvDm = new int[256];
    static long[] pvT = new long[256];

    static void keepAlive() {
        // 注意：整段必须 try 住 —— 这里抛任何异常都会冒泡到绘制线程，直接闪退
        try {
        long now = System.currentTimeMillis();
        // seen 必须按「最大容量」开：pvN 在本函数里会增长，
        // 用 new boolean[pvN] 会在新增条目后第二轮循环时 ArrayIndexOutOfBounds → 闪退
        boolean[] seen = new boolean[256];
        // 缓存满了就直接清空重来（容器/物资 120m 内不可能有 200 个，
        // 真到了也是极端场景，清一次比数组越界/无限增长安全）
        if (pvN >= 200) pvN = 0;
        for (int i = 0; i < txtN; i++) {
            int k = txtKind[i];
            if (k != 2 && k != 3) continue;                 // 只对容器/物资
            String nm = txtName[i] == null ? "" : txtName[i];
            String in = txtInst[i] == null ? "" : txtInst[i];
            // key 里【绝对不能】带屏幕位置：转视角时同一物件每帧坐标都在变，
            // 带位置就会把同一个箱子登记成十几个不同 key，600ms 内全部补画 → 拖出一串叠影。
            // 用「类型 + 实例名 + 类名」，实例名在关卡里唯一，位置每帧就地更新。
            if (in.length() == 0 && nm.length() == 0) continue;
            String key = k + "|" + in + "|" + nm;
            int hit = -1;
            for (int j = 0; j < pvN && j < 256; j++) {
                if (seen[j] || pvKey[j] == null) continue;
                if (pvKey[j].equals(key)) { hit = j; break; }
            }
            if (hit < 0) {
                if (pvN >= 256) continue;
                hit = pvN++;
                pvKey[hit] = key;
            }
            seen[hit] = true;
            pvX[hit] = txtX[i]; pvY[hit] = txtY[i]; pvDm[hit] = txtDm[i];
            pvKind[hit] = k; pvType[hit] = txtType[i];
            pvName[hit] = nm; pvInst[hit] = in; pvT[hit] = now;
        }
        for (int j = 0; j < pvN; j++) {
            if (seen[j]) continue;
            if (now - pvT[j] > KEEP_MS) continue;
            if (txtN >= 128) break;
            txtKind[txtN] = pvKind[j];
            txtType[txtN] = pvType[j];
            txtName[txtN] = pvName[j];
            txtInst[txtN] = pvInst[j];
            txtOpen[txtN] = 0; txtBotF[txtN] = -1;
            txtX[txtN] = pvX[j]; txtY[txtN] = pvY[j]; txtDm[txtN] = pvDm[j];
            txtN++;
            pvT[j] = now - KEEP_MS;        // 只补一次，别连续补
        }
        } catch (Throwable th) { pvN = 0; }        // 出错就清空缓存，绝不冒泡
    }

    static void loadEsp() {
        try {
            long mt = new java.io.File("/data/local/tmp/esp.txt").lastModified();
            if (mt != lastSeq) { lastSeq = mt; lastSeqMs = System.currentTimeMillis(); }
            BufferedReader br = new BufferedReader(new FileReader("/data/local/tmp/esp.txt"));
            // 文件成功打开 → 这一帧数据可信，才清空上一帧的标签
            boxN = 0; txtN = 0; boneN = 0; segN = 0;
            for (int ci = 0; ci < CAT_N; ci++) catNum[ci] = 0;
            String line;
            while ((line = br.readLine()) != null) {
                if (line.startsWith("s=")) {
                    String[] sp = line.substring(2).split(" ");
                    if (sp.length >= 4 && segN < 128) {
                        try {
                            sgX1[segN] = Float.parseFloat(sp[0]); sgY1[segN] = Float.parseFloat(sp[1]);
                            sgX2[segN] = Float.parseFloat(sp[2]); sgY2[segN] = Float.parseFloat(sp[3]);
                            segN++;
                        } catch (Throwable t) {}
                    }
                    continue;
                }
                if (line.startsWith("b=")) {
                    String[] bp2 = line.substring(2).split(" ");
                    if (bp2.length >= 3 && boneN < 256) {
                        try {
                            bnX[boneN] = Float.parseFloat(bp2[1]);
                            bnY[boneN] = Float.parseFloat(bp2[2]);
                            boneN++;
                        } catch (Throwable t) {}
                    }
                    continue;
                }
                if (line.startsWith("t=")) {
                    String[] tp = line.substring(2).split(" ");
                    if (tp.length >= 4 && txtN < 128) {
                        try {
                            txtKind[txtN] = Integer.parseInt(tp[0]);
                            txtType[txtN] = -1;
                            txtName[txtN] = (tp.length >= 5) ? tp[4] : "";
                            txtInst[txtN] = (tp.length >= 6) ? tp[5] : "";
                            txtOpen[txtN] = 0;
                            txtBotF[txtN] = -1;
                            if (boxN > 0) boxKind[boxN - 1] = txtKind[txtN];
                            // 只对「角色」(0/1/4) 读第 6 字段；物资/容器的第 6 字段是实例名（字符串），
                            // 误当成枚举会让物资被归成"玩家"，屏幕上就是一堆空气里的玩家框
                            int k0 = txtKind[txtN];
                            if ((k0 == 0 || k0 == 1 || k0 == 4) && tp.length >= 6) {
                                // 角色：第 6 字段 = *(u32*)(actor+0x1290)：1=人机 2=人机玩家 3=真人玩家
                                try { txtBotF[txtN] = Integer.parseInt(tp[5]); } catch (Throwable t) {}
                                if (tp.length >= 8) txtWeap[txtN] = tp[7];
                                if (tp.length >= 10) {
                                    try { txtArmLv[txtN] = Integer.parseInt(tp[8]); } catch (Throwable t) {}
                                    try { txtHelLv[txtN] = Integer.parseInt(tp[9]); } catch (Throwable t) {}
                                    // t= 行共 11 个字段：kind cx y dm cls ua tf100 wp 甲 头 名字
                                    // 名字是 tp[10]（之前误写成 tp[11] + length>=12，永远取不到）
                                    if (tp.length >= 11) txtPName[txtN] = tp[10];
                                }
                                int ua = txtBotF[txtN];
                                // actor+0x1290 三值枚举：1=人机  2=人机玩家  3=真人玩家（实测）
                                boolean modelBot = (txtKind[txtN] == 1);
                                if (ua == 1)      txtKind[txtN] = 1;
                                else if (ua == 2) txtKind[txtN] = 4;
                                else if (ua == 3) txtKind[txtN] = 0;
                                else              txtKind[txtN] = modelBot ? 1 : 0;   // 读不到就退回类名
                            }
                            if (txtKind[txtN] == 2 && tp.length >= 5) {
                                // 类名 + 实例名一起喂给关键词匹配：有些容器类名是通用的
                                // BP_ContainerBase_C，类型信息可能只在实例名里
                                String hint = (tp.length >= 6) ? tp[5] : "";
                                int ci = ctypeOf(tp[4], hint);
                                if (ci < 0) ci = catOf(tp[4] + "|" + hint);
                                txtType[txtN] = ci;
                                // 第 8 字段：RootComponent+0xD8，1=已打开
                                if (tp.length >= 8) {
                                    try { txtOpen[txtN] = Integer.parseInt(tp[7]); } catch (Throwable t) {}
                                }
                                if (ci >= 0 && ci < CAT_N) catNum[ci]++;
                                if (boxN > 0) boxType[boxN - 1] = ci;
                            }
                            txtX[txtN] = Float.parseFloat(tp[1]);
                            txtY[txtN] = Float.parseFloat(tp[2]);
                            txtDm[txtN] = Integer.parseInt(tp[3]);
                            txtN++;
                        } catch (Throwable t) {}
                    }
                    continue;
                }
                if (line.startsWith("a=") || line.startsWith("b=")) {
                    String[] parts = line.substring(2).split(" ");
                    if (parts.length >= 5 && boxN < 32) {
                        try {
                            if (line.charAt(0) == 'a') {
                                boxX[boxN] = Float.parseFloat(parts[1]);
                                boxY[boxN] = Float.parseFloat(parts[2]);
                                boxW[boxN] = Float.parseFloat(parts[3]);
                                boxH[boxN] = Float.parseFloat(parts[4]);
                                boxN++;
                            }
                        } catch (Throwable t) {}
                    }
                    continue;
                }
                int eq = line.indexOf('=');
                if (eq < 0) continue;
                String k = line.substring(0, eq), v = line.substring(eq + 1);
                if (k.equals("pid")) espPid = v; else if (k.equals("base")) espBase = v;
                else if (k.equals("state")) espState = v;
                else if (k.equals("reads")) espReads = v; else if (k.equals("loops")) espLoops = v;
                else if (k.startsWith("a=")) { } // 忽略
                else if (k.equals("entry")) { ms[0] = v; }
                else if (k.equals("pl")) { try { plA2 = Integer.parseInt(v.trim()); } catch (Throwable t) {} }
                else if (k.equals("p260")) { try { plB2 = Integer.parseInt(v.trim()); } catch (Throwable t) {} }
                else if (k.equals("realpl")) { try { realPl = Integer.parseInt(v.trim()); } catch (Throwable t) {} }
                else if (k.equals("enc3")) { try { enc3 = Integer.parseInt(v.trim()); } catch (Throwable t) {} }
                else if (k.equals("enc4")) { try { rawEnc4 = Integer.parseInt(v.trim()); } catch (Throwable t) {} }
                else if (k.equals("enc")) {
                    int sl = v.indexOf('/');
                    if (sl > 0) {
                        try { encHit = Integer.parseInt(v.substring(0, sl).trim()); } catch (Throwable t) {}
                        try { encTot = Integer.parseInt(v.substring(sl + 1).trim()); } catch (Throwable t) {}
                    }
                }
                else if (k.equals("phoff")) { ms[1] = v; }
                else if (k.equals("phnum")) { ms[2] = v; }
                else if (k.equals("p0vaddr")) { ms[3] = v; }
            }
            br.close();
        } catch (Throwable t) {}
        keepAlive();
        if (plA2 >= 0) settlePl(plA2, plB2 < 0 ? 0 : plB2);
    }
    // ================= 触摸坐标换算 =================
    // 实测 touchpanel 量程：ABS_MT_POSITION_X max 19840, Y max 44352
    // 而这两个值是**竖屏**（1080x2414）坐标系，overlay 是横屏 2414x1080，必须换轴。
    static final float RAW_X_MAX = 19840.0f, RAW_Y_MAX = 44352.0f;
    static final int PORTRAIT_W = 1080, PORTRAIT_H = 2414;
    static volatile int rotMode = -1;      // 0..3，-1=未定
    // ---- 三点仿射标定：lx = a*rx + b*ry + c ; ly = d*rx + e*ry + f ----
    static volatile int calStep = 0;        // 0..2=等三个点, 3=已标定
    static volatile int[] calRx = new int[3], calRy = new int[3];
    static volatile boolean affineOk = false;
    static volatile float A_a, A_b, A_c, A_d, A_e, A_f;
    static volatile int lastLX = -1, lastLY = -1;   // 最近一次点击的屏幕坐标（画十字用）
    // ---- 开关（真正参与绘制）----
    static volatile int[] kindOn = { 1, 1, 1, 1, 1 };  // 0=玩家 1=人机 2=容器 3=物资 4=人机玩家
    static volatile int boneOn = 1, labelOn = 1;
    static volatile int armorBarOn = 1;             // 护甲等级条
    static volatile int fpsNow = 0;                 // 实测每秒绘制帧数
    static volatile int plAll = 0, pl260 = 0;
    static volatile int encHit = 0, encTot = 0;   // 坐标加密：0x47C 命中数 / 真人总数（保留诊断）
    static volatile int enc3 = -1;                 // 加密判定：1 加密 / 0 正常 / -1 未知
    static volatile int rawEnc4 = -1;              // 采集端原始判定（未去抖）
    static volatile int enc4 = -1;                 // 去抖后的显示值
    static int enc4Last = -2; static long enc4Ms = 0;
    static volatile int aceN = 0;                  // ACE 目录 5 秒内被写过的文件数
    static volatile String aceName = "-";
    static volatile int realPl = 0;                // 全图剩余玩家（GameState.PlayerArray，不受 cull 限制）
    // 人数显示滞后：客户端 streaming 会让偶发丢帧时数字跳一下，稳定 1.2 秒才采用
    static int plA2 = -1, plB2 = -1; static long plMs = 0;
    static void settlePl(int a, int b) {
        long now = System.currentTimeMillis();
        if (a != plA2 || b != plB2) { plA2 = a; plB2 = b; plMs = now; return; }
        if (now - plMs >= 1200) { plAll = plA2; pl260 = plB2; }
    }       // 全图玩家数 / 260m 内玩家数
    // 物资标注：类别中文 + 物品名 + 单价，只标 60 米内、最多 40 个，免得满屏花
    static final int ITEM_COL = 0xFFB7F7FF;
    static final int ITEM_MAX_M = 120, ITEM_MAX_N = 40;
    // 物资价格挡位（只显示单价 >= 挡位 的物资）
    static final int[] PRICE_TIERS = { 2000, 5000, 10000, 20000, 25000 };
    static volatile int priceSel = 2000;
    // 字形图集（任意中文）：char -> {x,y,w,h,adv}
    static Object glyphBmp = null;
    static final java.util.HashMap<Character,int[]> glyphR = new java.util.HashMap<>();
    // 物资中文名表 / 容器类别表（都可热改）
    static final java.util.ArrayList<String> nKeys = new java.util.ArrayList<>();
    static final java.util.ArrayList<String> nVals = new java.util.ArrayList<>();
    static final java.util.ArrayList<String> lvKeys = new java.util.ArrayList<>();
    static final java.util.ArrayList<Integer> lvVals = new java.util.ArrayList<>();
    static volatile long lvLoadMs = 0;
    static final java.util.ArrayList<String> cKeys = new java.util.ArrayList<>();
    static final java.util.ArrayList<String> cVals = new java.util.ArrayList<>();
    static volatile long nameLoadMs = 0, ctypeLoadMs = 0, skipLoadMs = 0;
    static final java.util.ArrayList<String> skKeys = new java.util.ArrayList<>();
    static volatile int nameLastN = -1; static volatile long nameFileLen = -1;
    static final java.util.ArrayList<String>  pKeys = new java.util.ArrayList<>();
    static final java.util.ArrayList<Integer> pVals = new java.util.ArrayList<>();
    static final java.util.HashMap<String,Integer> priceCache = new java.util.HashMap<>();
    static volatile long priceLoadMs = 0;
    static volatile int priceLastN = -1; static volatile long priceFileLen = -1;
    // ---- 命中区（每帧重建，UI 与判定同源）----
    static final int[][] hitR = new int[40][5];     // x,y,w,h,id
    static volatile int hitN = 0;
    static void hit(int x, int y, int w, int h, int id) {
        if (hitN < hitR.length) { int[] r = hitR[hitN++]; r[0]=x; r[1]=y; r[2]=w; r[3]=h; r[4]=id; }
    }
    static long lastTapMs = 0;
    static final int[][] CAL_T = { {150,150}, {2264,150}, {150,930} };   // TL / TR / BL
    // 去重：一次抬手会同时产生 BTN_TOUCH(330)=0 与 ABS_MT_TRACKING_ID(57)=-1 两个事件
    static volatile long lastTapAt = 0;
    static volatile int lastTapRx = -9999, lastTapRy = -9999;
    static final int T1X = 220,  T1Y = 200;
    static final int T2X = 2194, T2Y = 880;

    // 把原始触摸值转成横屏屏幕坐标（四种旋转各试一次）
    static int[] mapTouch(int rx, int ry, int mode) {
        int pxs = (int)(rx * (float)PORTRAIT_W / RAW_X_MAX);   // 竖屏 x 0..1080
        int pys = (int)(ry * (float)PORTRAIT_H / RAW_Y_MAX);   // 竖屏 y 0..2414
        // 竖屏 (pxs 0..1080, pys 0..2414) → 横屏 (lx 0..2414, ly 0..1080)
        // 四种旋转/翻转组合；实际用哪种由"首次点击是否落在面板内"自动判定
        int lx, ly;
        switch (mode) {
            case 0: lx = pys;       ly = pxs;       break;   // 90° CCW
            case 1: lx = pys;       ly = H - pxs;   break;   // 90° CW
            case 2: lx = W - pys;   ly = pxs;       break;   // 270° CW
            default:lx = W - pys;   ly = H - pxs;   break;   // 270° CCW
        }
        return new int[]{lx, ly};
    }
    static int[] toScreen(int rx, int ry) {
        if (affineOk) {
            int lx = (int)(A_a * rx + A_b * ry + A_c);
            int ly = (int)(A_d * rx + A_e * ry + A_f);
            return new int[]{lx, ly};
        }
        return mapTouch(rx, ry, rotMode < 0 ? 0 : rotMode);
    }

    // 用三点解仿射：返回 false 表示退化
    static boolean solveAffine() {
        double x1=calRx[0], y1=calRy[0], x2=calRx[1], y2=calRy[1], x3=calRx[2], y3=calRy[2];
        double det = x1*(y2-y3) - y1*(x2-x3) + (x2*y3 - x3*y2);
        if (Math.abs(det) < 1e-6) return false;
        double lx1=CAL_T[0][0], lx2=CAL_T[1][0], lx3=CAL_T[2][0];
        double ly1=CAL_T[0][1], ly2=CAL_T[1][1], ly3=CAL_T[2][1];
        // lx = a*rx + b*ry + c
        A_a = (float)((lx1*(y2-y3) + lx2*(y3-y1) + lx3*(y1-y2)) / det);
        A_b = (float)((x1*(lx2-lx3) + x2*(lx3-lx1) + x3*(lx1-lx2)) / det);
        A_c = (float)((x1*(y2*lx3 - y3*lx2) + x2*(y3*lx1 - y1*lx3) + x3*(y1*lx2 - y2*lx1)) / det);
        // ly = d*rx + e*ry + f
        A_d = (float)((ly1*(y2-y3) + ly2*(y3-y1) + ly3*(y1-y2)) / det);
        A_e = (float)((x1*(ly2-ly3) + x2*(ly3-ly1) + x3*(ly1-ly2)) / det);
        A_f = (float)((x1*(y2*ly3 - y3*ly2) + x2*(y3*ly1 - y1*ly3) + x3*(y1*ly2 - y2*ly1)) / det);
        affineOk = true;
        return true;
    }

    static void onTap(int rx, int ry) {
        if (rx < 0 || ry < 0) return;
        long now = System.currentTimeMillis();
        if (now - lastTapAt < 350 && Math.abs(rx - lastTapRx) < 60 && Math.abs(ry - lastTapRy) < 60) return;
        lastTapAt = now; lastTapRx = rx; lastTapRy = ry;
        if (calStep < 3) {                       // 三点标定
            // 与上一点太近则视为重复事件
            for (int k = 0; k < calStep; k++) {
                if (Math.abs(rx - calRx[k]) < 400 && Math.abs(ry - calRy[k]) < 400) {
                    System.out.println("[CAL] dup too close to p" + (k+1) + ", ignored");
                    return;
                }
            }
            calRx[calStep] = rx; calRy[calStep] = ry;
            System.out.println("[CAL] p" + (calStep+1) + " raw=" + rx + "," + ry
                               + " (want " + CAL_T[calStep][0] + "," + CAL_T[calStep][1] + ")");
            calStep++;
            if (calStep == 3) {
                if (solveAffine()) {
                    saveCfg();
                    System.out.println("[CAL] affine a=" + A_a + " b=" + A_b + " c=" + A_c
                                       + " d=" + A_d + " e=" + A_e + " f=" + A_f);
                } else {
                    System.out.println("[CAL] degenerate, retry");
                    calStep = 0;
                }
            }
            return;
        }
        int[] sp = toScreen(rx, ry);
        int x = sp[0], y = sp[1];
        lastLX = x; lastLY = y;
        boolean wasMove = tapMoved; tapMoved = false; scrolling = false;
        tapCount++;
        System.out.println("[TAP] raw=" + rx + "," + ry + " -> " + x + "," + y + " rot=" + rotMode + " local=" + (x-PX) + "," + (y-PY)
                           + " scroll=" + scrollY + " max=" + maxScroll + " moved=" + wasMove);
        float lx = x - PX, ly = y - PY;
        if (wasMove) return;                       // 滑动过就不触发点击
        int lyB = (int)(y - PY - TITLE_H + scrollY);   // 命中区以"面板体"坐标记录
        for (int i = 0; i < hitN; i++) {
            int[] r = hitR[i];
            if (lx >= r[0] && lx <= r[0] + r[2] && lyB >= r[1] && lyB <= r[1] + r[3]) {
                switch (r[4]) {
                    case 1:  kindOn[0] ^= 1; break;
                    case 2:  kindOn[1] ^= 1; break;
                    case 3:  kindOn[2] ^= 1; break;
                    case 7:  kindOn[3] ^= 1; break;
                    case 8:  kindOn[4] ^= 1; break;
                    case 9:  armorBarOn ^= 1; break;
                    case 4:  boneOn ^= 1;    break;
                    case 5:  labelOn ^= 1;   break;
                    case 6:  contExpand = !contExpand; scrollY = 0; break;
                    case 99: userMoved = false; scrollY = 0;
                             newPX = (W - PW) / 2; newPY = (H - PH) / 2; needMove = true; break;
                    default:
                        if (r[4] >= 20 && r[4] < 20 + FPS_OPTS.length) {
                            fpsSel = FPS_OPTS[r[4] - 20];
                        } else if (r[4] >= 30 && r[4] < 30 + PRICE_TIERS.length) {
                            priceSel = PRICE_TIERS[r[4] - 30];
                        } else if (r[4] >= 100 && r[4] < 100 + CAT_N) {
                            int ci = r[4] - 100; catOn[ci] = (catOn[ci] == 1) ? 0 : 1;
                        }
                }
                saveCfg();
                System.out.println("[UI] id=" + r[4]);
                return;
            }
        }
    }

    // 自清理：本机 su 只给 uid0 不给 caps，adb 侧杀不掉 root 悬浮窗，
    // 所以由新实例在启动时收拾旧实例，保证只有一个在跑。
    // 先「投毒」ov_kill 让旧实例自己 System.exit（自杀一定成功），
    // 再对不支持的旧版本发 SIGKILL，最多重试 3 轮。
    static void killStaleOverlays() {
        // 只在 /data/local/tmp/ov_killcol 存在时才连采集端一起清（避免普通重启把数据层干掉）
        boolean killCol = new java.io.File("/data/local/tmp/ov_killcol").exists();
        java.io.File kf = new java.io.File("/data/local/tmp/ov_kill");
        try { if (!kf.exists()) kf.createNewFile(); } catch (Throwable t) {}
        try { Thread.sleep(1200); } catch (Throwable t) {}
        try { kf.delete(); } catch (Throwable t) {}
        for (int round = 0; round < 3; round++) {
            int left = signalKillStale(killCol);
            if (left == 0) break;
            try { Thread.sleep(400); } catch (Throwable t) {}
        }
        if (killCol) {
            try { new java.io.File("/data/local/tmp/ov_killcol").delete(); } catch (Throwable t) {}
            try { Thread.sleep(600); } catch (Throwable t) {}
        }
    }
    static int signalKillStale(boolean killCol) {
        int left = 0;
        try {
            java.io.File selfF = new java.io.File("/proc/self");
            String self = selfF.getCanonicalPath();
            self = self.substring(self.lastIndexOf('/') + 1);
            java.io.File[] ps = new java.io.File("/proc").listFiles();
            if (ps == null) return 0;
            for (java.io.File p : ps) {
                String n = p.getName();
                if (n.length() == 0 || n.charAt(0) < '0' || n.charAt(0) > '9') continue;
                if (n.equals(self)) continue;
                String cmd = "";
                try {
                    java.io.FileInputStream fis = new java.io.FileInputStream("/proc/" + n + "/cmdline");
                    byte[] b = new byte[1024];
                    int r = fis.read(b); fis.close();
                    if (r > 0) cmd = new String(b, 0, r, "UTF-8").replace('\0', ' ');
                } catch (Throwable t) { continue; }
                boolean isOv = cmd.contains("app_process") && cmd.matches("(?s).*\\bOv\\d+\\b.*");
                // 采集端现在会伪装进程名并抹掉 cmdline，所以改看 /proc/pid/exe
                String exe = "";
                try { exe = new java.io.File("/proc/" + n + "/exe").getCanonicalPath(); } catch (Throwable t) {}
                String comm = "";
                try {
                    java.io.FileInputStream cf = new java.io.FileInputStream("/proc/" + n + "/comm");
                    byte[] cb = new byte[64]; int cr = cf.read(cb); cf.close();
                    if (cr > 0) comm = new String(cb, 0, cr, "UTF-8").trim();
                } catch (Throwable t) {}
                boolean isCol = cmd.contains("paradise_fast") || exe.contains("paradise_fast")
                             || comm.equals("kworker/u16:2");
                if (!isOv && !(killCol && isCol)) continue;
                left++;
                try {
                    Process kp = Runtime.getRuntime().exec(new String[]{ "/system/bin/kill", "-9", n });
                    int rc = kp.waitFor();
                    if (rc != 0) {
                        // 有些进程域不同，普通信号被拒；临时 permissive 后再试
                        try { Runtime.getRuntime().exec(new String[]{ "/system/bin/setenforce", "0" }).waitFor(); } catch (Throwable t) {}
                        Process kp2 = Runtime.getRuntime().exec(new String[]{ "/system/bin/kill", "-9", n });
                        rc = kp2.waitFor();
                        try { Runtime.getRuntime().exec(new String[]{ "/system/bin/setenforce", "1" }).waitFor(); } catch (Throwable t) {}
                        System.out.println("[*] kill(permissive) pid=" + n + " rc=" + rc);
                    } else {
                        System.out.println("[*] kill stale overlay pid=" + n + " rc=0");
                    }
                } catch (Throwable t) { System.out.println("[!] kill " + n + " : " + t); }
                try {
                    Class<?> pc = Class.forName("android.os.Process");
                    pc.getMethod("sendSignal", int.class, int.class).invoke(null, Integer.parseInt(n), 9);
                } catch (Throwable t) { System.out.println("[!] sendSignal " + n + " : " + t); }
            }
            if (left > 0) Thread.sleep(300);
        } catch (Throwable t) { System.out.println("[!] killStaleOverlays: " + t); }
        return left;
    }

    static String shortName(String n) {
        String s2 = n;
        if (s2.startsWith("BP_")) s2 = s2.substring(3);
        if (s2.endsWith("_C")) s2 = s2.substring(0, s2.length()-2);
        if (s2.length() > 24) s2 = s2.substring(0, 24);
        return s2;
    }
    // 只接受真正的 UE 类名，避免配置文件里混进垃圾键（调试残留）
    static boolean looksLikeClass(String n) {
        if (n == null || n.length() < 4) return false;
        return n.endsWith("_C") || n.startsWith("BP_") || n.startsWith("Inventory")
               || n.indexOf('_') > 0;
    }
    // 容器类名 → 中文名图集 key。命中不到返回 null，用 ASCII 兜底。
    // 依据：游戏内容器类名形如 Inventory_071_SafeBox_C，中段是语义词，按关键词归类。
    static String zhKey(String raw) {
        if (raw == null) return null;
        String s = raw.toLowerCase();
        // 保险箱要排在最前：SafeBox 里也带 "box"
        if (s.contains("safebox") || s.contains("safe_box")) return "n_safebox";
        // 盒子：先判是不是人机盒（Scav/NPC 掉的），否则按玩家盒
        if (s.contains("deathbox") || s.contains("death_box") || s.contains("corpse")
            || s.contains("deadbody") || s.contains("dead_body") || s.contains("bodybox")) {
            // 反过来了：**默认人机盒**，只有明确写 player/pmc 才当玩家盒。
            // 依据：盒子里 AI 占绝大多数，且 AI 阵营名千奇百怪（Gendarmerie 宪兵 /
            // Rogue 游荡者 / Raider / Boss / Guard / Cultist …），
            // 以前「不命中关键词就默认玩家盒」会把高级人机盒子全标成玩家盒子。
            if (s.contains("player") || s.contains("pmc")) return "n_box_player";
            return "n_box_bot";
        }
        if (s.contains("jewel")) return "n_jewelry";
        if (s.contains("weapon")) return "n_weapon";
        if (s.contains("ammo")) return "n_ammo";
        if (s.contains("medic") || s.contains("_med") || s.contains("firstaid")) return "n_med";
        if (s.contains("tool")) return "n_tool";
        if (s.contains("duffel") || s.contains("backpack") || s.contains("_bag")) return "n_bag";
        if (s.contains("drawer")) return "n_drawer";
        if (s.contains("wardrobe") || s.contains("closet")) return "n_wardrobe";
        if (s.contains("computer") || s.contains("_pc") || s.contains("machinecase")) return "n_pc";
        if (s.contains("filing") || s.contains("folder") || s.contains("document")) return "n_file";
        if (s.contains("cash") || s.contains("register")) return "n_cash";
        if (s.contains("fridge") || s.contains("refriger")) return "n_fridge";
        if (s.contains("washer")) return "n_washer";
        if (s.contains("suitcase") || s.contains("briefcase")) return "n_case";
        if (s.contains("airdrop")) return "n_airdrop";
        if (s.contains("locker") || s.contains("cabinet")) return "n_locker";
        if (s.contains("jacket") || s.contains("clothes")) return "n_jacket";
        if (s.contains("crate") || s.contains("lootbox") || s.contains("_box")) return "n_crate";
        return null;
    }

    // 写一张表用的"等级"字段：头盔/护甲/背包 等装备的等级（可在 ov_level.txt 里改）
    static int levelOf(String cls, String inst) {
        String s = ((cls == null ? "" : cls) + "|" + (inst == null ? "" : inst)).toLowerCase();
        int best = -1, bestLen = -1;
        synchronized (lvKeys) {
            for (int i = 0; i < lvKeys.size(); i++) {
                String k = lvKeys.get(i);
                if (k.length() >= bestLen && s.contains(k)) { bestLen = k.length(); best = lvVals.get(i); }
            }
        }
        return best;
    }
    // 装备槽位：0=头盔 1=护甲 2=背包/弹挂，-1=不是可穿戴防具
    static int wearSlot(String cls, String inst) {
        String k = itemTypeKey((cls == null ? "" : cls) + " " + (inst == null ? "" : inst));
        if (k.equals("t_helmet")) return 0;
        if (k.equals("t_vest"))   return 1;
        if (k.equals("t_bag") || k.equals("t_rig")) return 2;
        return -1;
    }

    // ---- 全局标签排版：任何两个标签在 x 上有交集且 y 太近，就把后面的往下挪（像列表）----
    static final int[] ocX0 = new int[192], ocY0 = new int[192];
    static final int[] ocX1 = new int[192], ocY1 = new int[192];
    static int ocN = 0;
    static int ocPlace(int x, int w, int y, int h) {
        for (int guard = 0; guard < 40; guard++) {
            boolean hit = false;
            for (int q = 0; q < ocN; q++) {
                boolean xo = !(x + w < ocX0[q] || x > ocX1[q]);
                boolean yo = !(y + h < ocY0[q] || y > ocY1[q]);
                if (xo && yo) { hit = true; y = ocY1[q] + 4; break; }
            }
            if (!hit) break;
        }
        if (ocN < 192) {
            ocX0[ocN] = x; ocY0[ocN] = y; ocX1[ocN] = x + w; ocY1[ocN] = y + h;
            ocN++;
        }
        return y;
    }

    // 标签绘制：三遍走
    //   ① 收集角色（玩家/人机/人机玩家）
    //   ② 找出「穿在角色身上」的头甲背包（屏幕位置与角色重合 + 距离相同），只把等级记到角色上
    //   ③ 画：角色（含等级）/ 容器 / 地上的物资（位置接近时纵向堆叠，不重叠）
    static void drawLabels(Object c) throws Exception {
        ocN = 0;
        int[] chTxt = new int[128];
        int[] chSlotLv = new int[32 * 3];
        int chN = 0;
        for (int ti = 0; ti < txtN && chN < 32; ti++) {
            int tk = txtKind[ti];
            if (tk != 0 && tk != 1 && tk != 4) continue;
            if (kindOn[tk] == 0) continue;
            chTxt[chN] = ti;
            chSlotLv[chN * 3] = -1; chSlotLv[chN * 3 + 1] = -1; chSlotLv[chN * 3 + 2] = -1;
            // 采集端从「耐久曲线资产名」解析出来的等级最可靠，先用它
            if (txtHelLv[ti] > 0) chSlotLv[chN * 3] = txtHelLv[ti];
            if (txtArmLv[ti] > 0) chSlotLv[chN * 3 + 1] = txtArmLv[ti];
            chN++;
        }

        int[] worn = new int[128];
        int[] items = new int[64];
        int itemN = 0;
        for (int ti = 0; ti < txtN; ti++) {
            if (txtKind[ti] != 3) continue;
            if (kindOn[3] == 0) continue;
            if (txtDm[ti] > ITEM_MAX_M) continue;
            String cls = txtName[ti] == null ? "" : txtName[ti];
            String ins = txtInst[ti] == null ? "" : txtInst[ti];
            if (skipped(cls, ins)) continue;        // 忽略表：地图物件被误认成物资时用
            int pr = priceOf(cls, ins);
            // ② 穿戴判定必须放在价格过滤**之前**：否则便宜的护甲/头盔等级永远算不出来
            int slot = wearSlot(cls, ins);
            if (slot >= 0 && chN > 0) {
                int bestC = -1, bestD = 9999;
                for (int ci = 0; ci < chN; ci++) {
                    int ct = chTxt[ci];
                    if (Math.abs(txtDm[ct] - txtDm[ti]) > 2) continue;
                    int dx = Math.abs((int) txtX[ct] - (int) txtX[ti]);
                    int dy = Math.abs((int) txtY[ct] - (int) txtY[ti]);
                    if (dx <= 40 && dy <= 110) { int d2 = dx + dy; if (d2 < bestD) { bestD = d2; bestC = ci; } }
                }
                if (bestC >= 0) {
                    int lv = levelOf(cls, ins);
                    if (lv > chSlotLv[bestC * 3 + slot]) chSlotLv[bestC * 3 + slot] = lv;
                    worn[ti] = 1;
                    continue;                    // 穿戴物不再单独画
                }
            }
            if (pr < priceSel) continue;         // 非穿戴物才按价格挡位过滤
            if (itemN < 64) items[itemN++] = ti;
        }

        // ③ 角色
        for (int ci = 0; ci < chN; ci++) {
            int ti = chTxt[ci];
            int tk = txtKind[ti];
            String nm2 = (tk == 1) ? "人机" : ((tk == 4) ? "人机玩家" : "玩家");
            int tcol = (tk == 1) ? 0xFFFFAA00 : ((tk == 4) ? 0xFFB36BFF : 0xFFFF5050);
            // 真人优先显示玩家名字（RC 同款），拿不到才退回「玩家」
            String pnm = txtPName[ti];
            String lab;
            if (tk == 0 && pnm != null && pnm.length() > 0 && !"-".equals(pnm)) lab = pnm;
            else lab = nm2;
            // 头甲不再写进主标签 —— 下面「武器行」已经显示「头X 甲X 包X 武器」，
            // 两处都写会重复。主标签只留「名字 + 距离」。
            lab = lab + " " + txtDm[ti] + "M";
            int lx = (int) txtX[ti], ly = (int) txtY[ti];
            int wch = Math.max(gwidth(lab), 90);
            ly = ocPlace(lx - wch / 2, wch, ly - 24, 66);
            if (glyphBmp != null) gtext(c, lab, lx - gwidth(lab) / 2, ly, tcol);
            else text(c, lab, lx - 40, ly + 8, 2, tcol);
            // 穿戴等级：头X 甲X 包X
            StringBuilder eq = new StringBuilder();
            String[] tag = { "头", "甲", "包" };
            for (int sl = 0; sl < 3; sl++) {
                int lv = chSlotLv[ci * 3 + sl];
                if (lv <= 0) continue;                 // 没读到等级就不写
                if (eq.length() > 0) eq.append(' ');
                eq.append(tag[sl]).append(lv);
            }
            String wn = weaponName(txtWeap[ti]);       // 手持武器
            if (wn != null) {
                if (eq.length() > 0) eq.append("  ");
                eq.append(wn);
            }
            // 护甲等级条（竖条，左边缘，1~6 级按高度填充 + 按等级变色）
            if (armorBarOn == 1) {
                int lvMax = Math.max(chSlotLv[ci * 3], chSlotLv[ci * 3 + 1]);   // 头 / 甲
                if (lvMax > 0) {
                    int bx = (int) txtX[ti] - 58, byy = (int) txtY[ti] - 32;
                    int BH = 64;
                    rect(c, bx, byy, bx + 9, byy + BH, 0xAA000000);
                    int fill = BH * Math.min(lvMax, 6) / 6;
                    int lc;
                    if (lvMax <= 2)      lc = 0xFF9CCC65;
                    else if (lvMax == 3) lc = 0xFFFFD24A;
                    else if (lvMax == 4) lc = 0xFFFF9A3C;
                    else if (lvMax == 5) lc = 0xFFFF5252;
                    else                 lc = 0xFFC77DFF;
                    rect(c, bx, byy + BH - fill, bx + 9, byy + BH, lc);
                }
            }
            if (eq.length() > 0) {
                String es = eq.toString();
                int ecol = 0xFFD8D8D8;
                if (glyphBmp != null) gtext(c, es, lx - gwidth(es) / 2, ly + 30, ecol);
                else text(c, es, lx - 20, ly + 44, 2, ecol);
            }
        }

        // ③b 容器
        for (int ti = 0; ti < txtN; ti++) {
            if (txtKind[ti] != 2) continue;
            if (kindOn[2] == 0) continue;
            int ci2 = txtType[ti];
            if (ci2 < 0 || ci2 >= CAT_N) ci2 = CAT_N - 1;
            if (catOn[ci2] == 0) continue;
            // 「穿在角色身上」的容器（vestContainer / MyContainer / Pocket）本身不是世界物资，
            // 角色被 cull 之后它就孤零零挂在半空 —— 这类一律不画
            String ins2 = txtInst[ti] == null ? "" : txtInst[ti];
            if (ins2.contains("vestContainer") || ins2.contains("MyContainer")
                || ins2.contains("Pocket")) continue;
            // 背包/弹挂容器如果和某个「盒子」重合，说明是尸体身上掉出来的，不再单独画
            if (CAT_KEY[ci2].equals("n_bag")) {
                boolean inBox = false;
                for (int tj = 0; tj < txtN && !inBox; tj++) {
                    if (txtKind[tj] != 2 || tj == ti) continue;
                    int cj = txtType[tj];
                    if (cj < 0 || cj >= CAT_N) continue;
                    String kj = CAT_KEY[cj];
                    if (!kj.startsWith("n_box")) continue;
                    if (Math.abs(txtDm[tj] - txtDm[ti]) <= 1
                        && Math.abs((int) txtX[tj] - (int) txtX[ti]) <= 30
                        && Math.abs((int) txtY[tj] - (int) txtY[ti]) <= 60) inBox = true;
                }
                if (inBox) continue;
            }
            int ccol = CAT_COL[ci2];
            int[] rr0 = uiR.get(CAT_KEY[ci2]);
            int cw = (rr0 != null ? rr0[2] : 80) + 8 + (txtOpen[ti] == 1 ? 34 : 0) + 56;
            int lx2 = (int) txtX[ti] - cw / 2;
            int ly2 = ocPlace(lx2, cw, (int) txtY[ti] - 16, 56);
            label(c, CAT_KEY[ci2], lx2, ly2, ccol);
            int wl = (rr0 != null) ? rr0[2] : 80;
            if (txtOpen[ti] == 1) {                       // 已打开：后面备注一个「开」
                int oc = 0xFFFFB300;
                if (glyphBmp != null) gtext(c, "开", lx2 + wl + 8, ly2 - 1, oc);
                else text(c, "OPEN", lx2 + wl + 8, ly2 + 4, 2, oc);
            }
            text(c, txtDm[ti] + "M", lx2 + cw - 56, ly2 + 18, 2, ccol);
        }

        // ③c 地上物资：按屏幕 y 排序后做防重叠堆叠
        for (int i = 1; i < itemN; i++) {          // 插入排序
            int v = items[i], j = i - 1;
            while (j >= 0 && txtY[items[j]] > txtY[v]) { items[j + 1] = items[j]; j--; }
            items[j + 1] = v;
        }
        final int LINE_H = 30;
        for (int ii = 0; ii < itemN; ii++) {
            int ti = items[ii];
            String cls = txtName[ti] == null ? "" : txtName[ti];
            String ins = txtInst[ti] == null ? "" : txtInst[ti];
            int pr = priceOf(cls, ins);
            String disp = (cls.indexOf("ItemBase") >= 0 && ins.length() > 2 && !ins.equals("-")) ? ins : cls;
            String zh = nameOf(cls, ins);
            if (zh == null) zh = typeNameOf(cls, ins);
            String nm = (zh != null) ? zh : shortItem(disp);
            String ps = (pr > 0) ? ("" + pr) : "?";
            String ds = txtDm[ti] + "M";
            int col = (pr >= 20000) ? 0xFFFFD24A : ((pr >= 10000) ? 0xFFB6FF3C : ITEM_COL);
            int w1 = gwidth(nm), w2 = gwidth(ps), w3 = gwidth(ds);
            int totalW = w1 + 10 + w2 + 10 + w3;
            int ix = (int) txtX[ti];
            int x0g = ix - totalW / 2;
            int y0g = ocPlace(x0g, totalW, (int) txtY[ti] - 12, LINE_H);
            if (glyphBmp != null) {
                gtext(c, nm, x0g, y0g, col);
                gtext(c, ps, x0g + w1 + 10, y0g, col);
                gtext(c, ds, x0g + w1 + 10 + w2 + 10, y0g, col);
            } else {
                text(c, nm, x0g, y0g, 2, col);
                text(c, ps, x0g + w1 + 10, y0g, 2, col);
                text(c, ds, x0g + w1 + 10 + w2 + 10, y0g, 2, col);
            }
        }
    }

    // ---------- 物资：类别 / 名字 / 价格 ----------
    static String itemTypeKey(String n) {
        if (n == null) return "n_item";
        String s = n.toLowerCase();
        if (s.contains("helmet")) return "t_helmet";
        if (s.contains("vest") || s.contains("armor")) return "t_vest";
        if (s.contains("headset") || s.contains("headphone")) return "t_headset";
        if (s.contains("backpack") || s.contains("_bag")) return "t_bag";
        if (s.contains("rig")) return "t_rig";
        if (s.contains("gpu") || s.contains("graphic") || s.contains("gold") || s.contains("jewel")
            || s.contains("bitcoin") || s.contains("ledx") || s.contains("cash")) return "t_valuable";
        if (s.contains("adapter") || s.contains("magazine") || s.contains("_mag") || s.contains("scope")
            || s.contains("sight") || s.contains("silenc") || s.contains("muzzle")
            || s.contains("handguard") || s.contains("stock") || s.contains("grip")) return "t_attach";
        if (s.contains("ammo") || s.contains("_mm") || s.contains("bullet") || s.contains("buck")) return "t_ammo";
        if (s.contains("medkit") || s.contains("bandage") || s.contains("medical") || s.contains("_med")
            || s.contains("pill") || s.contains("drug") || s.contains("splint")
            || s.contains("tourniquet")) return "t_med";
        if (s.contains("key")) return "t_key";
        return "n_item";
    }
    // 物品短名：剥掉 BP_ / _C / _Uam 之类前后缀
    static String shortItem(String n) {
        if (n == null) return "?";
        String s = n;
        if (s.startsWith("BP_")) s = s.substring(3);
        if (s.endsWith("_C")) s = s.substring(0, s.length() - 2);
        if (s.endsWith("_Uam")) s = s.substring(0, s.length() - 4);
        s = s.replace("_Uam_", "_");
        if (s.endsWith("_")) s = s.substring(0, s.length() - 1);
        if (s.length() > 14) s = s.substring(0, 14);
        return s;
    }
    // ---------- 字形图集：画任意中文 ----------
    static void loadGlyphAtlas() {
        try {
            Class<?> bfCls = Class.forName("android.graphics.BitmapFactory");
            glyphBmp = bfCls.getMethod("decodeFile", String.class).invoke(null, "/data/local/tmp/glyph.png");
            if (glyphBmp == null) { System.out.println("[!] glyph.png 缺失，中文名不可用"); return; }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream("/data/local/tmp/glyph.txt"), "UTF-8"));
            String ln;
            while ((ln = br.readLine()) != null) {
                String[] p = ln.trim().split(" +");
                if (p.length < 6) continue;
                try {
                    int code = Integer.parseInt(p[0], 16);
                    glyphR.put((char) code, new int[] {
                        Integer.parseInt(p[1]), Integer.parseInt(p[2]),
                        Integer.parseInt(p[3]), Integer.parseInt(p[4]),
                        Integer.parseInt(p[5]) });
                } catch (Throwable t) {}
            }
            br.close();
            System.out.println("[*] 字形图集 " + glyphR.size() + " 字");
        } catch (Throwable t) { System.out.println("[!] glyph: " + t); }
    }
    static int gwidth(String s) {
        if (s == null) return 0;
        int w = 0;
        for (int i = 0; i < s.length(); i++) {
            int[] g = glyphR.get(s.charAt(i));
            if (g != null) w += g[4];
        }
        return w;
    }
    static void gtext(Object c, String s, int x, int y, int color) {
        if (glyphBmp == null || s == null) return;
        try {
            mSetColor.invoke(paint, color);
            mSetAA.invoke(paint, true);
            Object cf = cfOf(color);
            if (cf != null && mSetCF != null) mSetCF.invoke(paint, cf);
            for (int i = 0; i < s.length(); i++) {
                int[] g = glyphR.get(s.charAt(i));
                if (g == null) continue;
                // 图集每个字形都是 30x34 单元格，但 ASCII 的 advance 只有 8~11，
                // 按单元格宽画会让字母互相压住 —— 窄字符用 advance 当绘制宽度
                int dw = (g[4] < 20) ? (g[4] + 2) : g[2];
                Object sr = cRect.newInstance(g[0], g[1], g[0] + g[2], g[1] + g[3]);
                Object dr = cRect.newInstance(x, y, x + dw, y + g[3]);
                mDrawBmp.invoke(c, glyphBmp, sr, dr, paint);
                x += g[4];
            }
            if (mSetCF != null) mSetCF.invoke(paint, new Object[]{null});
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }

    // ---------- 物资中文名表 ----------
    static final String NAME_FILE = "/data/local/tmp/ov_name.txt";
    static final String NAME_DEFAULT =
        "# 物资中文名：关键字(小写, 子串匹配)  中文名\n" +
        "# 后面的行覆盖前面的；改完 5 秒自动重载\n" +
        "# --- 护甲 / 头盔 / 耳机 ---\n" +
        "vest_3m 3M防弹衣\nmulticam 迷彩防弹衣\nshpm SHPM头盔\nuntar UNTAR头盔\n" +
        "kiverm Kiver头盔\nkirasa Kirasa防弹衣\nfort FORT防弹衣\nssh68 SSh68头盔\n" +
        "comtac2 ComTac2耳机\nsn_42 SN42防弹衣\n" +
        "# --- 武器 / 弹药 ---\n" +
        "aks74u AKS74U\nakm AKM\nmp133 MP133\nm9_bayonet M9刺刀\n" +
        "m855a1 5.56M855A1\npso 9x19PSO\n" +
        "# --- 背包 / 弹挂 ---\n" +
        "armybag 军用背包\nduffle 行李袋\nsling 单肩包\nmbss MBSS背包\n" +
        "tbag T型背包\nsfmp SFMP背包\nd3crx D3CRX弹挂\n6sh112 6SH112背包\n" +
        "mrig MRig弹挂\nwtirg WTRig弹挂\n6b3tm01m 6B3TM胸挂\n" +
        "# --- 贵重 / 杂物 ---\n" +
        "gpu 显卡\ngraphic 显卡\nledx LEDX\nbitcoin 比特币\ngold 金条\njewel 珠宝\n" +
        "harddrive 硬盘\nbattery 电池\nfuse 保险丝\nwire 电线\n" +
        "# --- 武器 ---\n" +
        "ak74n AK74N\nglock18c 格洛克18C\nmp5 MP5\nsuomi 索米KP31\n" +
        "m1911 M1911\nsks SKS\nm700 M700\nvector 维克托\np90 P90\nm249 M249\n" +
        "# --- 投掷物 / 弹药 / 其它（实测类名）---\n" +
        "stungrenade 闪光弹\nsmokegrenade 烟雾弹\nzarya 扎里亚闪光弹\nm18 M18烟雾弹\n" +
        "762x39 7.62x39弹\n9x19 9x19弹\n556x45 5.56x45弹\n545x39 5.45x39弹\n" +
        "guwu 古董钥匙\ntacticalsport 战术运动耳机\nssh40 SSH40钢盔\n" +
        "hat_ssh40 SSH40钢盔\nadapter 武器配件\nthrowweapon 投掷物\n" +
        "# --- 弹匣（按容量/型号）---\n" +
        "6l20 6L20弹匣\n6l31 6L31弹匣\n30r 30发弹匣\n45r 45发弹匣\n" +
        "60r 60发弹匣\n95r 95发弹鼓\n_mag 弹匣\nmagazine 弹匣\n";
    static void loadNameTable(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - nameLoadMs < 5000) return;
        nameLoadMs = now;
        java.io.File f = new java.io.File(NAME_FILE);
        try {
            if (!f.exists()) {
                java.io.FileWriter w = new java.io.FileWriter(f);
                w.write(NAME_DEFAULT); w.close();
            }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(f), "UTF-8"));
            java.util.ArrayList<String> k2 = new java.util.ArrayList<>();
            java.util.ArrayList<String> v2 = new java.util.ArrayList<>();
            String ln;
            while ((ln = br.readLine()) != null) {
                ln = ln.trim();
                if (ln.length() < 3 || ln.charAt(0) == '#') continue;
                int sp = ln.indexOf(' ');
                if (sp < 1) sp = ln.indexOf('\t');
                if (sp < 1) continue;
                String k = ln.substring(0, sp).trim().toLowerCase();
                String v = ln.substring(sp + 1).trim();
                if (k.length() > 0 && v.length() > 0) { k2.add(k); v2.add(v); }
            }
            br.close();
            synchronized (nKeys) { nKeys.clear(); nVals.clear(); nKeys.addAll(k2); nVals.addAll(v2); }
            if (k2.size() != nameLastN || f.length() != nameFileLen) {
                nameLastN = k2.size(); nameFileLen = f.length();
                System.out.println("[*] 物资名表 " + k2.size() + " 条");
            }
        } catch (Throwable t) { System.out.println("[!] 名表: " + t); }
    }
    static String nameOf(String cls, String inst) {
        String s = ((cls == null ? "" : cls) + "|" + (inst == null ? "" : inst)).toLowerCase();
        String best = null;
        synchronized (nKeys) {
            for (int i = 0; i < nKeys.size(); i++) if (s.contains(nKeys.get(i))) best = nVals.get(i);
        }
        return best;
    }

    // 手持武器名：优先中文名表，否则从类名里剥出型号
    static String weaponName(String cls) {
        if (cls == null || cls.length() < 3 || cls.equals("-")) return null;
        // +0x148 有时指向的不是武器（可能是刚装备的甲/配件），只认武器类
        String lc = cls.toLowerCase();
        if (!lc.contains("weapon") && !lc.contains("pistolgrip")) return null;
        String zh = nameOf(cls, "");
        if (zh != null) return zh;
        String s = cls;
        if (s.endsWith("_0")) s = s.substring(0, s.length() - 2);      // 实例序号
        if (s.startsWith("BP_")) s = s.substring(3);
        if (s.endsWith("_C")) s = s.substring(0, s.length() - 2);
        if (s.endsWith("_Uam")) s = s.substring(0, s.length() - 4);
        if (s.startsWith("Weapon_")) s = s.substring(7);
        if (s.startsWith("pistolgrip_")) s = s.substring(11);
        if (s.length() > 14) s = s.substring(0, 14);
        return s.length() > 0 ? s : null;
    }

    // ---------- 物资忽略表：命中的东西一律不画（比如地图上的出生点被误认成物品）----------
    static final String SKIP_FILE = "/data/local/tmp/ov_skip.txt";
    static final String SKIP_DEFAULT =
        "# 物资忽略表：关键字(小写, 子串匹配) 命中就不绘制\n" +
        "# 用于过滤「被误认成物资的地图物件」，改完 5 秒生效\n" +
        "guwu\n" +
        "spawnpoint\n" +
        "playerstart\n" +
        "birthpoint\n" +
        "bullet\n" +          // 子弹实体满地都是，没价值
        "\u624b\u96f7\n";
    static void loadSkipTable(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - skipLoadMs < 5000) return;
        skipLoadMs = now;
        java.io.File f = new java.io.File(SKIP_FILE);
        try {
            if (!f.exists()) {
                java.io.FileWriter w = new java.io.FileWriter(f);
                w.write(SKIP_DEFAULT); w.close();
            }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(f), "UTF-8"));
            java.util.ArrayList<String> k2 = new java.util.ArrayList<>();
            String ln;
            while ((ln = br.readLine()) != null) {
                ln = ln.trim().toLowerCase();
                if (ln.length() < 2 || ln.charAt(0) == '#') continue;
                k2.add(ln);
            }
            br.close();
            synchronized (skKeys) { skKeys.clear(); skKeys.addAll(k2); }
        } catch (Throwable t) {}
    }
    static boolean skipped(String cls, String inst) {
        String s = ((cls == null ? "" : cls) + "|" + (inst == null ? "" : inst)).toLowerCase();
        synchronized (skKeys) {
            for (int i = 0; i < skKeys.size(); i++) if (s.contains(skKeys.get(i))) return true;
        }
        return false;
    }

    // 名表查不到时，至少给个中文类型名（不再显示英文）
    static String typeNameOf(String cls, String inst) {
        String k = itemTypeKey((cls == null ? "" : cls) + " " + (inst == null ? "" : inst));
        if (k.equals("t_helmet"))   return "头盔";
        if (k.equals("t_vest"))     return "防弹衣";
        if (k.equals("t_headset"))  return "耳机";
        if (k.equals("t_bag"))      return "背包";
        if (k.equals("t_rig"))      return "弹挂";
        if (k.equals("t_gun"))      return "枪械";
        if (k.equals("t_ammo"))     return "弹药";
        if (k.equals("t_attach"))   return "枪械配件";
        if (k.equals("t_med"))      return "医疗品";
        if (k.equals("t_key"))      return "钥匙";
        if (k.equals("t_valuable")) return "贵重品";
        return "物资";
    }

    // ---------- 装备等级表 ----------
    static final String LEVEL_FILE = "/data/local/tmp/ov_level.txt";
    static final String LEVEL_DEFAULT =
        "# 装备等级：关键字(小写, 子串匹配)  等级   —— 最长关键字优先\n" +
        "# 传奇/史诗这类品级或护甲等级都行，数字会原样显示成「头4 甲3 包2」\n" +
        "# --- 头盔 ---\n" +
        "ssh40 1\nssh68 2\nuntar 3\nkiverm 3\nshpm 4\n" +
        "# --- 护甲 ---\n" +
        "vest_3m 2\nmulticam 3\nkirasa 3\nfort 4\nsn_42 2\n" +
        "# --- 背包 / 弹挂 ---\n" +
        "sling 1\nduffle 2\nd3crx 2\nmrig 2\nwtirg 2\n6b3tm01m 2\n" +
        "armybag 3\nmbss 3\ntbag 3\nsfmp 3\n6sh112 4\n";
    static void loadLevelTable(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - lvLoadMs < 5000) return;
        lvLoadMs = now;
        java.io.File f = new java.io.File(LEVEL_FILE);
        try {
            if (!f.exists()) {
                java.io.FileWriter w = new java.io.FileWriter(f);
                w.write(LEVEL_DEFAULT); w.close();
            }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(f), "UTF-8"));
            java.util.ArrayList<String> k2 = new java.util.ArrayList<>();
            java.util.ArrayList<Integer> v2 = new java.util.ArrayList<>();
            String ln;
            while ((ln = br.readLine()) != null) {
                ln = ln.trim();
                if (ln.length() < 3 || ln.charAt(0) == '#') continue;
                String[] p = ln.split("[\\s]+");
                if (p.length < 2) continue;
                try { k2.add(p[0].toLowerCase()); v2.add(Integer.parseInt(p[1])); } catch (Throwable t) {}
            }
            br.close();
            synchronized (lvKeys) { lvKeys.clear(); lvVals.clear(); lvKeys.addAll(k2); lvVals.addAll(v2); }
            System.out.println("[*] 装备等级表 " + k2.size() + " 条");
        } catch (Throwable t) { System.out.println("[!] 等级表: " + t); }
    }

    // ---------- 容器类别覆盖表 ----------
    static final String CTYPE_FILE = "/data/local/tmp/ov_ctype.txt";
    static final String CTYPE_DEFAULT =
        "# 容器类别判定：关键字(小写, 子串匹配)  类别key\n" +
        "# 后面的行覆盖前面的；类别key 见面板里列的 22 类\n" +
        "# 通用基类 BP_ContainerBase_C / BP_RewardBox_C 的名字里没有类型，\n" +
        "# 靠实例名区分，实测：vestContainer/ChestRigs=弹挂背包，MyContainer_*=背包\n" +
        "# 通用规则写前面\n" +
        "deathboxplayer n_box_player\ndeathboxpmc n_box_player\nplayerbox n_box_player\n" +
        "deathbox n_box_bot\ncorpse n_box_bot\nrewardbox n_box_player\n" +
        "safebox n_safebox\n" +
        "vestcontainer n_bag\nchestrigs n_bag\nmycontainer n_bag\n" +
        "# 具体规则写后面（最长关键字优先，所以越具体越赢）\n" +
        "deathboxscav n_box_bot\nscav n_box_bot\ngendarmerie n_box_bot\nrogue n_box_bot\nraider n_box_bot\n" +
        "armybag n_bag\nduffle n_bag\nsling n_bag\nrig n_bag\n" +
        "toolbox n_tool\nmedbox n_med\nammobox n_ammo\nweaponbox n_weapon\n" +
        "drawer n_drawer\nwardrobe n_wardrobe\nfridge n_fridge\nwasher n_washer\n" +
        "cashregister n_cash\nfiling n_file\ncomputer n_pc\njewel n_jewelry\n" +
        "airdrop n_airdrop\nlocker n_locker\ncabinet n_locker\ncrate n_crate\n";
    static void loadCtypeTable(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - ctypeLoadMs < 5000) return;
        ctypeLoadMs = now;
        java.io.File f = new java.io.File(CTYPE_FILE);
        try {
            if (!f.exists()) {
                java.io.FileWriter w = new java.io.FileWriter(f);
                w.write(CTYPE_DEFAULT); w.close();
            }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(f), "UTF-8"));
            java.util.ArrayList<String> k2 = new java.util.ArrayList<>();
            java.util.ArrayList<String> v2 = new java.util.ArrayList<>();
            String ln;
            while ((ln = br.readLine()) != null) {
                ln = ln.trim();
                if (ln.length() < 3 || ln.charAt(0) == '#') continue;
                String[] p = ln.split("[\\s]+");
                if (p.length < 2) continue;
                k2.add(p[0].toLowerCase()); v2.add(p[1]);
            }
            br.close();
            synchronized (cKeys) { cKeys.clear(); cVals.clear(); cKeys.addAll(k2); cVals.addAll(v2); }
            System.out.println("[*] 容器类别表 " + k2.size() + " 条");
        } catch (Throwable t) { System.out.println("[!] 类别表: " + t); }
    }
    static int catIndexOf(String key) {
        for (int i = 0; i < CAT_N; i++) if (CAT_KEY[i].equals(key)) return i;
        return -1;
    }
    static int ctypeOf(String cls, String inst) {
        String s = ((cls == null ? "" : cls) + "|" + (inst == null ? "" : inst)).toLowerCase();
        String best = null;
        int bestLen = -1;
        synchronized (cKeys) {
            // 用「最长关键字优先」而不是「后者覆盖」：
            // deathboxscav(13) 必须压过 deathbox(8)/scav(4)，否则人机盒会被判成玩家盒
            for (int i = 0; i < cKeys.size(); i++) {
                String k = cKeys.get(i);
                if (k.length() >= bestLen && s.contains(k)) { bestLen = k.length(); best = cVals.get(i); }
            }
        }
        if (best == null) return -1;
        return catIndexOf(best);
    }

    static final String PRICE_FILE = "/data/local/tmp/ov_price.txt";
    static final String PRICE_DEFAULT =
        "# 物资价格表：关键字(小写, 子串匹配)  单价\n" +
        "# 匹配规则：按文件顺序，**后面的行覆盖前面的** —— 通用规则写前面，具体物品写后面\n" +
        "# 改完保存，绘制端 5 秒内自动重载，不用重启\n" +
        "# 下面数值只是起步示例，请按当前市场价自行校准\n" +
        "--- 通用兜底（类型级）---\n" +
        "helmet 3000\nvest 5000\narmor 5000\nheadset 8000\nbackpack 4000\nrig 6000\n" +
        "bullet 500\nammo 500\nmagazine 2000\nadapter 2000\nscope 3000\nsight 3000\n" +
        "medkit 3000\nbandage 1200\nkey 5000\n" +
        "--- 具体物品（覆盖上面的兜底值）---\n" +
        "gpu 45000\ngraphic 45000\nledx 80000\nbitcoin 60000\ngold 40000\njewel 30000\n" +
        "m995 3000\n7n39 2800\nm61 2500\nm855 1200\n" +
        "fort 25000\ncomtac 20000\nkirasa 12000\nshpm 9000\nkiver 8000\nuntar 6000\nssh68 4000\n" +
        "harddrive 9000\nbattery 8000\nfuse 3000\nwire 2000\n";
    static void loadPriceTable(boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - priceLoadMs < 5000) return;
        priceLoadMs = now;
        java.io.File f = new java.io.File(PRICE_FILE);
        try {
            if (!f.exists()) {
                java.io.FileWriter w = new java.io.FileWriter(f);
                w.write(PRICE_DEFAULT); w.close();
                System.out.println("[*] 已生成价格表模板 " + PRICE_FILE);
            }
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(f), "UTF-8"));
            java.util.ArrayList<String>  k2 = new java.util.ArrayList<>();
            java.util.ArrayList<Integer> v2 = new java.util.ArrayList<>();
            String ln;
            while ((ln = br.readLine()) != null) {
                ln = ln.trim();
                if (ln.length() < 3 || ln.charAt(0) == '#') continue;
                String[] p = ln.split("[\\s]+");
                if (p.length < 2) continue;
                try {
                    int v = Integer.parseInt(p[1].replace(",", "").replace("\u00a5", ""));
                    if (p[0].length() > 0) { k2.add(p[0].toLowerCase()); v2.add(v); }
                } catch (Throwable t) {}
            }
            br.close();
            synchronized (pKeys) {
                pKeys.clear(); pVals.clear();
                pKeys.addAll(k2); pVals.addAll(v2);
                priceCache.clear();
            }
            // 只在内容变化时打日志，避免每 5 秒刷一行
            if (k2.size() != priceLastN || f.length() != priceFileLen) {
                priceLastN = k2.size(); priceFileLen = f.length();
                System.out.println("[*] 价格表载入 " + k2.size() + " 条 (文件已更新)");
            }
        } catch (Throwable t) { System.out.println("[!] 价格表: " + t); }
    }
    static int priceOf(String cls, String inst) {
        String key = (cls == null ? "" : cls) + "|" + (inst == null ? "" : inst);
        Integer c = priceCache.get(key);
        if (c != null) return c.intValue();
        String s = key.toLowerCase();
        int best = 0;
        synchronized (pKeys) {
            // 按文件顺序匹配，**后面的行覆盖前面的**（通用规则写前面，具体物品写后面）
            for (int i = 0; i < pKeys.size(); i++) {
                if (s.contains(pKeys.get(i))) best = pVals.get(i).intValue();
            }
        }
        priceCache.put(key, best);
        return best;
    }
    static void saveCfg() {
        try {
            StringBuilder sb = new StringBuilder();
            sb.append("master=").append(contMaster ? 1 : 0).append('\n');
            sb.append("k0=").append(kindOn[0]).append('\n');
            sb.append("k1=").append(kindOn[1]).append('\n');
            sb.append("k2=").append(kindOn[2]).append('\n');
            sb.append("k3=").append(kindOn[3]).append('\n');
            sb.append("k4=").append(kindOn[4]).append('\n');
            sb.append("bone=").append(boneOn).append('\n');
            sb.append("lab=").append(labelOn).append('\n');
            sb.append("abar=").append(armorBarOn).append('\n');
            sb.append("fps=").append(fpsSel).append('\n');
            sb.append("price=").append(priceSel).append('\n');
            sb.append("rot=").append(rotMode).append('\n');
            if (affineOk) sb.append(String.format("aff=%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n", A_a,A_b,A_c,A_d,A_e,A_f));
            for (int i = 0; i < CAT_N; i++) sb.append("cat_").append(CAT_KEY[i]).append('=').append(catOn[i]).append('\n');
            java.io.FileWriter fw = new java.io.FileWriter("/data/local/tmp/ov_cfg.txt");
            fw.write(sb.toString()); fw.close();
        } catch (Throwable t) {}
    }
    static void loadCfg() {
        try {
            BufferedReader br = new BufferedReader(new FileReader("/data/local/tmp/ov_cfg.txt"));
            String line;
            while ((line = br.readLine()) != null) {
                int eq = line.indexOf('=');
                if (eq <= 0) continue;
                String k = line.substring(0, eq), v = line.substring(eq + 1);
                if (k.equals("master")) { contMaster = v.equals("1"); continue; }
                if (k.equals("k0")) { kindOn[0] = Integer.parseInt(v); continue; }
                if (k.equals("k1")) { kindOn[1] = Integer.parseInt(v); continue; }
                if (k.equals("k2")) { kindOn[2] = Integer.parseInt(v); continue; }
                if (k.equals("k3")) { kindOn[3] = Integer.parseInt(v); continue; }
                if (k.equals("k4")) { kindOn[4] = Integer.parseInt(v); continue; }
                if (k.equals("bone")) { boneOn = Integer.parseInt(v); continue; }
                if (k.equals("lab")) { labelOn = Integer.parseInt(v); continue; }
                if (k.equals("abar")) { armorBarOn = Integer.parseInt(v); continue; }
                if (k.equals("fps")) { fpsSel = Integer.parseInt(v); continue; }
                if (k.equals("price")) { priceSel = Integer.parseInt(v); continue; }
                if (k.equals("rot")) { rotMode = Integer.parseInt(v); continue; }
                if (k.equals("aff")) {
                    String[] p6 = v.split(",");
                    if (p6.length == 6) {
                        A_a = Float.parseFloat(p6[0]); A_b = Float.parseFloat(p6[1]); A_c = Float.parseFloat(p6[2]);
                        A_d = Float.parseFloat(p6[3]); A_e = Float.parseFloat(p6[4]); A_f = Float.parseFloat(p6[5]);
                        affineOk = true; calStep = 3;
                    }
                    continue;
                }
                if (k.startsWith("cat_")) {
                    String ck = k.substring(4);
                    for (int i = 0; i < CAT_N; i++)
                        if (CAT_KEY[i].equals(ck)) { catOn[i] = v.equals("1") ? 1 : 0; break; }
                    continue;
                }
            }
            br.close();
        } catch (Throwable t) {}
    }

    public static void main(String[] a) throws Exception {
        killStaleOverlays();
        W = a.length > 0 ? Integer.parseInt(a[0]) : 760;
        H = a.length > 1 ? Integer.parseInt(a[1]) : 420;
        // 启动一律居中：面板宽 1440（屏宽 2414 的 ~60%），垂直居中
        int PH0 = 60 + 350;   // 标题 60 + 内容 348（紧凑双列 + 帧率行）
        PX = (W - PW) / 2;
        PY = (H - PH0) / 2;
        System.out.println("[*] 面板居中 pos=" + PX + "," + PY + " size=" + PW + "x" + PH0);

        Class<?> scCls = Class.forName("android.view.SurfaceControl");
        Class<?> bCls  = Class.forName("android.view.SurfaceControl$Builder");
        Class<?> txCls = Class.forName("android.view.SurfaceControl$Transaction");
        Class<?> surfCls = Class.forName("android.view.Surface");
        Class<?> rectCls = Class.forName("android.graphics.Rect");
        Class<?> canCls = Class.forName("android.graphics.Canvas");
        Class<?> pCls = Class.forName("android.graphics.Paint");

        Object b = bCls.getConstructor().newInstance();
        bCls.getMethod("setName", String.class).invoke(b, "uzi_selfbuild");
        bCls.getMethod("setBufferSize", int.class, int.class).invoke(b, W, H);
        bCls.getMethod("setFormat", int.class).invoke(b, 1);
        sc = bCls.getMethod("build").invoke(b);
        Object tx = txCls.getConstructor().newInstance();
        txCls.getMethod("setLayer", scCls, int.class).invoke(tx, sc, 0x7ffffff0);
        try { txCls.getMethod("setTrustedOverlay", scCls, boolean.class).invoke(tx, sc, true); } catch (Throwable t) {}
        try { txCls.getMethod("setPosition", scCls, float.class, float.class).invoke(tx, sc, (float)PX, (float)PY); } catch (Throwable t) {}
        try { txCls.getMethod("setVisibility", scCls, boolean.class).invoke(tx, sc, true); } catch (Throwable t) {}
        txCls.getMethod("apply").invoke(tx);

        surface = surfCls.getConstructor(scCls).newInstance(sc);
        mLock = surfCls.getMethod("lockCanvas", rectCls);
        mUnlock = surfCls.getMethod("unlockCanvasAndPost", canCls);
        mDrawColor = canCls.getMethod("drawColor", int.class);
        mDrawRect = canCls.getMethod("drawRect", float.class, float.class, float.class, float.class, pCls);
        try {
            Class<?> modeCls = Class.forName("android.graphics.PorterDuff$Mode");
            for (Object o : modeCls.getEnumConstants()) if ("CLEAR".equals(String.valueOf(o))) clearMode = o;
            mDrawColorMode = Class.forName("android.graphics.Canvas").getMethod("drawColor", int.class, modeCls);
            System.out.println("[*] PorterDuff.CLEAR ready");
        } catch (Throwable t) { System.out.println("[!] CLEAR unavailable: " + t); }
        paint = pCls.getConstructor().newInstance();
        mSetColor = pCls.getMethod("setColor", int.class);
        try {
            // 关键：必须显式 setTypeface(null)。不设置时 drawText 会去懒初始化默认字体，
            // 在 app_process 环境下触发 native assert（src == nullptr && gDefaultTypeface == nullptr）。
            Class<?> tfCls = Class.forName("android.graphics.Typeface");
            mSetTF = pCls.getMethod("setTypeface", tfCls);
            mSetTS = pCls.getMethod("setTextSize", float.class);
            mSave = canCls.getMethod("save");
            mRestore = canCls.getMethod("restore");
            mTranslate = canCls.getMethod("translate", float.class, float.class);
            mClip = canCls.getMethod("clipRect", float.class, float.class, float.class, float.class);
            // 用 Typeface.Builder 指定 ttc 索引加载系统 CJK 字体
            // （直接 createFromFile 对 .ttc 会返回 null，null 字体再 drawText 会 native assert）
            try {
                Class<?> bCls2 = Class.forName("android.graphics.Typeface$Builder");
                Object tfB = bCls2.getConstructor(java.io.File.class).newInstance(
                        new java.io.File("/system/fonts/NotoSansCJK-Regular.ttc"));
                try { bCls2.getMethod("setTtcIndex", int.class).invoke(tfB, 0); } catch (Throwable t2) {}
                tfCN = bCls2.getMethod("build").invoke(tfB);
                System.out.println("[*] 中文 Typeface=" + tfCN);
            } catch (Throwable t1) {
                System.out.println("[!] Typeface.Builder 失败: " + t1);
                tfCN = null;
            }
            mSetAA = pCls.getMethod("setAntiAlias", boolean.class);
            mDrawT = canCls.getMethod("drawText", String.class, float.class, float.class, pCls);
            System.out.println("[*] 中文字体就绪");
        } catch (Throwable t) { System.out.println("[!] 字体不可用: " + t); }
        pCls.getMethod("setAntiAlias", boolean.class).invoke(paint, false);
        pCls.getMethod("setStyle", Class.forName("android.graphics.Paint$Style")).invoke(paint, Class.forName("android.graphics.Paint$Style").getEnumConstants()[0]);

        for (int i = 0; i < 128; i++) { boxType[i] = -1; txtType[i] = -1; }
        // 默认只开 玩家盒 / 人机盒 / 保险箱 / 未知容器，其余等玩家自己勾
        for (int i = 0; i < CAT_N; i++) catOn[i] = 0;
        for (int k = 0; k < CAT_ON_DEFAULT.length; k++)
            for (int i = 0; i < CAT_N; i++)
                if (CAT_KEY[i].equals(CAT_ON_DEFAULT[k])) catOn[i] = 1;
        loadCfg();
        loadPriceTable(true);
        loadGlyphAtlas();
        loadNameTable(true);
        loadCtypeTable(true);
        loadLevelTable(true);
        loadSkipTable(true);
        try {
            Class<?> bfCls = Class.forName("android.graphics.BitmapFactory");
            uiBmp = bfCls.getMethod("decodeFile", String.class).invoke(null, "/data/local/tmp/ov_ui.png");
            BufferedReader fr = new BufferedReader(new FileReader("/data/local/tmp/ov_ui.txt"));
            String ln;
            while ((ln = fr.readLine()) != null) {
                String[] p = ln.trim().split(" ");
                if (p.length == 5) uiR.put(p[0], new int[]{ Integer.parseInt(p[1]), Integer.parseInt(p[2]),
                                                           Integer.parseInt(p[3]), Integer.parseInt(p[4]) });
            }
            fr.close();
            Class<?> bmpCls = Class.forName("android.graphics.Bitmap");
            Class<?> rectC2 = Class.forName("android.graphics.Rect");
            cRect = rectC2.getConstructor(int.class, int.class, int.class, int.class);
            mDrawBmp = canCls.getMethod("drawBitmap", bmpCls, rectC2, rectC2, pCls);
            System.out.println("[*] 中文图集就绪 " + uiR.size() + " 项 bmp=" + (uiBmp != null));
            try {
                Class<?> pdmCls = Class.forName("android.graphics.PorterDuff$Mode");
                for (Object o : pdmCls.getEnumConstants()) if ("SRC_IN".equals(String.valueOf(o))) cfSrcIn = o;
                cCF = Class.forName("android.graphics.PorterDuffColorFilter").getConstructor(int.class, pdmCls);
                mSetCF = pCls.getMethod("setColorFilter", Class.forName("android.graphics.ColorFilter"));
                System.out.println("[*] ColorFilter SRC_IN ready");
            } catch (Throwable t) { System.out.println("[!] ColorFilter 不可用: " + t); }
        } catch (Throwable t) { System.out.println("[!] 图集失败: " + t); }
        inputThread();
        // 图层位置强制归零：面板位置完全由画布内 PX/PY 控制，避免与拖拽叠加
        try {
            Object t0 = txCls.getConstructor().newInstance();
            txCls.getMethod("setPosition", scCls, float.class, float.class).invoke(t0, sc, 0.0f, 0.0f);
            txCls.getMethod("apply").invoke(t0);
            System.out.println("[*] layer pos -> 0,0");
        } catch (Throwable t) { System.out.println("[!] setPosition 失败: " + t); }
        System.out.println("[*] layer ready " + W + "x" + H + " @" + PX + "," + PY);

        long fpsT0 = System.currentTimeMillis(); int frameCnt = 0;
        for (int f = 0; ; f++) {
          // 整帧包 try：任何一帧抛异常都不许让绘制线程退出，
          // 否则用户看到的就是「进游戏一会就闪退」（线程死了没人画，表面还在）
          try {
            // 每秒统计一次真实绘制帧率（只统计本秒内画了多少帧）
            frameCnt++;
            {
                long nowMs = System.currentTimeMillis();
                if (nowMs - fpsT0 >= 1000) {
                    fpsNow = (int)(frameCnt * 1000L / Math.max(1L, nowMs - fpsT0));
                    frameCnt = 0; fpsT0 = nowMs;
                }
            }
            // 自杀开关：adb 侧 touch /data/local/tmp/ov_kill 即可让实例退出
            if ((f & 15) == 0) {
                java.io.File kf = new java.io.File("/data/local/tmp/ov_kill");
                if (kf.exists()) {
                    // 注意：不在这里删文件，交给新实例统一清理，
                    // 否则多个旧实例会互相抢删导致有的漏退。
                    System.out.println("[*] ov_kill 命中，退出");
                    System.exit(0);
                }
            }
            Object c = mLock.invoke(surface, new Object[]{null});
            if (c == null) { Thread.sleep(16); continue; }
            if (mDrawColorMode != null) mDrawColorMode.invoke(c, 0, clearMode);   // 整屏 CLEAR
            else mDrawColor.invoke(c, 0xFF000000);

            // 数据无条件加载：菜单隐掉时 ESP 仍要画
            loadPriceTable(false);      // 5 秒节流，改完价格表自动生效
            loadNameTable(false);
            loadCtypeTable(false);
            loadLevelTable(false);
            loadSkipTable(false);
            // 注意：标签数组的清理移到 loadEsp() 内部「文件打开成功之后」，
            // 否则 esp.txt 被采集端 rename 的瞬间读失败，会把整屏标签清空一帧 → 闪屏
            loadEsp();
            if (lastSeqMs != 0 && System.currentTimeMillis() - lastSeqMs > 1200) { boxN = 0; }

            // ---------- 菜单面板（VOL+/- 只隐藏这一块）----------
            if (!hidden) {
                hitN = 0;
                int catRows = (CAT_N + 2) / 3;
                int pad = 24, rowH = 44, colGap = 14;
                int colW = (PW - pad * 2 - colGap) / 2;
                int colW3 = (PW - pad * 2 - colGap * 2) / 3;
                int fpsH = 44, resetH = 40, FOOT_H = 40;
                int listH = contExpand ? catRows * rowH : 0;
                // 内容自然高度（不含底部状态栏）
                int contentH = 386 + listH;
                // 默认就开一块够高的面板（约 12 行可见），最多 740
                int bodyH = Math.min(Math.max(contentH + FOOT_H, 620), 780);
                PH = TITLE_H + bodyH;
                int viewH = bodyH - FOOT_H;                 // 可视内容区高度
                maxScroll = Math.max(0, contentH - viewH);
                if (scrollY > maxScroll) scrollY = maxScroll;
                if (!userMoved) PY = (H - PH) / 2;

                mSave.invoke(c);
                try {
                mTranslate.invoke(c, (float)PX, (float)PY);

                // 外框 + 标题
                rect(c, 0, 0, PW, PH, C_WIN);
                rect(c, 0, 0, PW, 3, C_BORDER);
                rect(c, 0, PH - 3, PW, PH, C_BORDER);
                rect(c, 0, 0, 3, PH, C_BORDER);
                rect(c, PW - 3, 0, PW, PH, C_BORDER);
                rect(c, 3, 3, PW - 3, TITLE_H - 3, C_TITLE);
                rect(c, 3, TITLE_H - 5, PW - 3, TITLE_H - 3, C_BORDER);
                label(c, "brand", 16, 10, 0xFFF0F0F0);
                label(c, "title2", 172, 10, C_DIM);
                rect(c, PW - 140, 22, PW - 128, 34, 0xFF25E6A0);
                label(c, "run", PW - 116, 12, 0xFF7FD8FF);

                // 面板体（裁剪 + 滚动）
                mSave.invoke(c);
                try {
                    mClip.invoke(c, 3f, (float)(TITLE_H + 2), (float)(PW - 3), (float)(TITLE_H + viewH));
                    mTranslate.invoke(c, 0f, (float)(TITLE_H - scrollY));

                    int x0 = pad, y0 = 8;
                    checkbox(c, "p1", kindOn[0] == 1, x0, y0, colW, 1);
                    checkbox(c, "p2", kindOn[1] == 1, x0 + colW + colGap, y0, colW, 2);
                    y0 += rowH;
                    checkbox(c, "p3", kindOn[2] == 1, x0, y0, colW, 3);
                    checkbox(c, "bone", boneOn == 1, x0 + colW + colGap, y0, colW, 4);
                    y0 += rowH;
                    checkbox(c, "dist", labelOn == 1, x0, y0, colW, 5);
                    checkbox(c, "p4", kindOn[3] == 1, x0 + colW + colGap, y0, colW, 7);
                    y0 += rowH;
                    checkbox(c, "p5", kindOn[4] == 1, x0, y0, colW, 8);
                    checkbox(c, "p6", armorBarOn == 1, x0 + colW + colGap, y0, colW, 9);
                    y0 += rowH;

                    // 帧率档位
                    label(c, "fps", x0, y0 + 8, C_DIM);
                    int bx = x0 + 76, bw2 = 84, bgap = 8;
                    for (int fi = 0; fi < FPS_OPTS.length; fi++) {
                        boolean sel = (fpsSel == FPS_OPTS[fi]);
                        rect(c, bx, y0 + 2, bx + bw2, y0 + 40, sel ? C_CHECK : C_FRAME);
                        text(c, "" + FPS_OPTS[fi], bx + 22, y0 + 12, 3, sel ? 0xFF0A1A24 : C_TEXT);
                        hit(bx, y0, bw2, 42, 20 + fi);
                        bx += bw2 + bgap;
                    }
                    y0 += fpsH;

                    // 物资价格挡位：只画单价 >= 挡位 的物资
                    label(c, "price", x0, y0 + 8, C_DIM);
                    int px3 = x0 + 76, bw3 = 110, bgap3 = 8;
                    for (int pi2 = 0; pi2 < PRICE_TIERS.length; pi2++) {
                        boolean sel = (priceSel == PRICE_TIERS[pi2]);
                        rect(c, px3, y0 + 2, px3 + bw3, y0 + 40, sel ? C_CHECK : C_FRAME);
                        text(c, "" + PRICE_TIERS[pi2], px3 + 12, y0 + 12, 3, sel ? 0xFF0A1A24 : C_TEXT);
                        hit(px3, y0, bw3, 42, 30 + pi2);
                        px3 += bw3 + bgap3;
                    }
                    y0 += fpsH;

                    rect(c, x0, y0, PW - pad, y0 + 2, C_SEP);
                    y0 += 12;
                    label(c, "cty", x0, y0 + 8, C_DIM);
                    int catTotal = 0;
                    for (int i = 0; i < CAT_N; i++) catTotal += catNum[i];
                    text(c, "" + catTotal, x0 + 112, y0 + 10, 3, C_DIM);
                    rect(c, PW - pad - 104, y0, PW - pad, y0 + 38, C_FRAME);
                    label(c, contExpand ? "col" : "exp", PW - pad - 92, y0 + 6, C_TEXT);
                    hit(PW - pad - 108, y0 - 4, 112, 46, 6);
                    y0 += rowH;

                    if (contExpand) {
                        // 类别三列排布：21 个类型只占 7 行，一屏全放下
                        for (int ci = 0; ci < CAT_N; ci++) {
                            int cc = ci % 3, cr = ci / 3;
                            int cx = x0 + cc * (colW3 + colGap);
                            int cy = y0 + cr * rowH;
                            boolean on = catOn[ci] == 1;
                            int col = on ? CAT_COL[ci] : 0xFF6B757E;
                            rect(c, cx, cy + 13, cx + 9, cy + 31, col);      // 类别色标
                            label(c, CAT_KEY[ci], cx + 22, cy + 6, on ? C_TEXT : C_DIM);
                            if (catNum[ci] > 0) text(c, "" + catNum[ci], cx + 136, cy + 12, 3, col);
                            int bx2 = cx + colW3 - 30, by2 = cy + 6;
                            rect(c, bx2, by2, bx2 + 26, by2 + 26, on ? C_CHECK : C_FRAME);
                            rect(c, bx2, by2, bx2 + 26, by2 + 2, C_BORDER);
                            rect(c, bx2, by2 + 24, bx2 + 26, by2 + 26, C_BORDER);
                            rect(c, bx2, by2, bx2 + 2, by2 + 26, C_BORDER);
                            rect(c, bx2 + 24, by2, bx2 + 26, by2 + 26, C_BORDER);
                            hit(cx - 6, cy, colW3 + 6, 40, 100 + ci);
                        }
                        y0 += catRows * rowH;
                    }
                    y0 += 10;
                    rect(c, x0, y0, PW - pad, y0 + resetH, C_BTN);
                    label(c, "reset", x0 + 14, y0 + 6, C_TEXT);
                    hit(x0, y0, PW - 2 * pad, resetH, 99);
                } finally { mRestore.invoke(c); }

                if (maxScroll > 0) {
                    int barH = Math.max(36, viewH * viewH / (viewH + maxScroll));
                    int barY = (int)((float)scrollY / maxScroll * (viewH - barH));
                    rect(c, PW - 11, TITLE_H + 4 + barY, PW - 5, TITLE_H + 4 + barY + barH, 0x77FFFFFF);
                }

                // 底部状态栏（固定不滚动）
                int fy = PH - FOOT_H + 8;
                rect(c, 3, PH - FOOT_H, PW - 3, PH - 3, C_TITLE);
                rect(c, 3, PH - FOOT_H, PW - 3, PH - FOOT_H + 2, C_BORDER);
                label(c, "goal", pad, fy, C_DIM);
                text(c, "" + boxN, pad + 62, fy + 4, 3, C_DIM);
                // 玩家：全图总数 / 300 米内
                String pls = "玩家 " + plAll + "  /260m " + pl260;
                if (glyphBmp != null) gtext(c, pls, pad + 130, fy + 2, 0xFFFF6B6B);
                else text(c, pls, pad + 130, fy + 4, 3, 0xFFFF6B6B);
                // 实时帧率（每秒刷新一次的真实绘制 FPS，不是累计帧数）
                text(c, "FPS " + fpsNow, pad + 520, fy + 4, 3,
                     fpsNow >= 55 ? 0xFF6EE7A0 : (fpsNow >= 30 ? 0xFFFFD24A : 0xFFFF6B6B));
                text(c, "PID " + espPid, pad + 700, fy + 4, 3, C_DIM);
                } finally { mRestore.invoke(c); }
            }

            // 加密状态去抖：连续 1 秒一致才更新，避免读数抖动
            {
                long now2 = System.currentTimeMillis();
                if (rawEnc4 != enc4Last) { enc4Last = rawEnc4; enc4Ms = now2; }
                else if (now2 - enc4Ms >= 1000) { enc4 = rawEnc4; }
            }

            // ---------- 左上角：坐标加密状态（常驻） ----------
            if (glyphBmp != null) {
                String es; int ec;
                if (enc4 == 1)      { es = "坐标 加密中"; ec = 0xFFFF4D4D; }
                else if (enc4 == 0) { es = "坐标 正常";   ec = 0xFF6EE7A0; }
                else if (aceN > 0)  { es = "ACE 活动 " + aceN; ec = 0xFFFFD24A; }
                else                { es = "坐标 未知";   ec = 0xFF888888; }
                int ew = gwidth(es);
                rect(c, 20, 20, 20 + ew + 24, 20 + 42, 0x99000000);
                rect(c, 20, 20, 20 + ew + 24, 22, ec);
                gtext(c, es, 32, 26, ec);
            }

            // ---------- 顶部常驻信息条（菜单隐藏也显示）----------
            if (glyphBmp != null) {
                // 全图人数用 GameState.PlayerArray（真实剩余人数）；拿不到才退回 cull 内计数
                int allN = (realPl > 0) ? realPl : plAll;
                String top = "全图 " + allN + "   260m 内 " + pl260;
                int tw = gwidth(top);
                int bx0 = W / 2 - tw / 2 - 16, by0 = 10;
                rect(c, bx0, by0, bx0 + tw + 32, by0 + 40, 0x99000000);
                rect(c, bx0, by0, bx0 + tw + 32, by0 + 2, 0xFFFF6B6B);
                gtext(c, top, W / 2 - tw / 2, by0 + 7, 0xFFFF6B6B);
            }

            // ---------- ESP 永远绘制 ----------
            for (int bi = 0; bi < boxN; bi++) {
                float x = boxX[bi], y = boxY[bi];
                float bw = boxW[bi] > 0 ? boxW[bi] : 70;
                float bh = boxH[bi] > 0 ? boxH[bi] : 140;
                float bx0 = x - bw/2, bx1 = x + bw/2, by0 = y - bh/2, by1 = y + bh/2;
                int k = (bi < boxKind.length) ? boxKind[bi] : 0;
                if (k >= 0 && k <= 4 && kindOn[k] == 0) continue;      // 玩家/人机/容器/物资/人机玩家
                // 容器/物资都不画方框：位置由下面的「彩色中文标签」表示
                if (k == 2 || k == 3) continue;
                int bc = (k == 1) ? 0xFFFFAA00 : ((k == 4) ? 0xFFB36BFF : 0xFFFF3030);
                rect(c, bx0, by0, bx1, by0 + 2, bc);
                rect(c, bx0, by1 - 2, bx1, by1, bc);
                rect(c, bx0, by0, bx0 + 2, by1, bc);
                rect(c, bx1 - 2, by0, bx1, by1, bc);
                float cxs = 1207f, cys = 540f;
                float ddx = x - cxs, ddy = y - cys;
                float dl = (float)Math.sqrt(ddx*ddx + ddy*ddy);
                if (dl > 1f) {
                    float ux = ddx/dl, uy = ddy/dl;
                    for (float t = 0; t <= 200f && t <= dl; t += 4f) {
                        float lx2 = cxs + ux*t, ly2 = cys + uy*t;
                        rect(c, lx2-1.5f, ly2-1.5f, lx2+1.5f, ly2+1.5f, 0x1E231C0F);
                    }
                }
            }
            if (boneOn == 1) for (int si = 0; si < segN; si++) {
                float x1 = sgX1[si], y1 = sgY1[si], x2 = sgX2[si], y2 = sgY2[si];
                float dxs = x2 - x1, dys = y2 - y1;
                int steps = (int)(Math.max(Math.abs(dxs), Math.abs(dys)) / 3.0f) + 1;
                if (steps > 120) steps = 120;
                for (int t2 = 0; t2 <= steps; t2++) {
                    float tt = (float) t2 / steps;
                    float px2 = x1 + dxs * tt, py2 = y1 + dys * tt;
                    rect(c, px2 - 1.5f, py2 - 1.5f, px2 + 1.5f, py2 + 1.5f, 0xAFFFFFFF);
                }
            }
            if (boneOn == 1) for (int bi2 = 0; bi2 < boneN; bi2++) {
                float bxx = bnX[bi2], byy = bnY[bi2];
                rect(c, bxx-2, byy-2, bxx+2, byy+2, 0xFFE8E8E8);
            }
            if (labelOn == 1) drawLabels(c);

            // ---- 标定靶 ----
            if (calStep < 3) {
                rect(c, 0, 0, W, H, 0x60000000);
                for (int i = 0; i < 3; i++) {
                    int cx1 = CAL_T[i][0], cy1 = CAL_T[i][1];
                    boolean active = (i == calStep);
                    int col = active ? 0xFF00FFC0 : 0xFF446655;
                    for (int rr = 70; rr < 110; rr += 8) {
                        for (int deg = 0; deg < 360; deg += 8) {
                            float rad = (float)(deg * Math.PI / 180.0);
                            float px3 = cx1 + (float)Math.cos(rad) * rr;
                            float py3 = cy1 + (float)Math.sin(rad) * rr;
                            rect(c, px3-2, py3-2, px3+2, py3+2, col);
                        }
                    }
                    text(c, "TAP " + (i+1), cx1 - 34, cy1 - 10, 2, active ? 0xFFFFFFFF : 0xFF668877);
                }
                text(c, "CAL " + (calStep+1) + "/3", W/2 - 90, 40, 3, 0xFFFFAA00);
                text(c, "TL  then  TR  then  BL", W/2 - 220, 84, 2, 0xFFCCCCCC);
            }
            // ---- 最近点击位置十字（标定时用，关闭以免干扰）----
            if (SHOW_TAP_CROSS && lastLX >= 0 && lastLY >= 0) {
                rect(c, lastLX-18, lastLY-1, lastLX+18, lastLY+1, 0xFFFF00FF);
                rect(c, lastLX-1, lastLY-18, lastLX+1, lastLY+18, 0xFFFF00FF);
            }
            mUnlock.invoke(surface, c);
            // 注意：不再对 SurfaceControl 做 setVisibility，
            // 否则菜单隐藏会把 ESP 一起藏掉（VOL+/- 现在只切菜单绘制）
            if (needMove) {
                needMove = false;
                PX = newPX; PY = newPY;   // 只改画布内偏移，图层始终在 0,0
            }
            if (f % 300 == 0) {
                StringBuilder dbg = new StringBuilder();
                int shown = 0;
                for (int ti = 0; ti < txtN && shown < 3; ti++) {
                    if (txtKind[ti] != 3) continue;
                    String c0 = txtName[ti] == null ? "" : txtName[ti];
                    String i0 = txtInst[ti] == null ? "" : txtInst[ti];
                    String z0 = nameOf(c0, i0);
                    dbg.append(" [").append(c0).append('/').append(i0).append("=>")
                       .append(z0 == null ? "?" : z0).append(':').append(priceOf(c0, i0)).append(']');
                    shown++;
                }
                StringBuilder dbg2 = new StringBuilder();
                int sh2 = 0;
                for (int ti = 0; ti < txtN && sh2 < 3; ti++) {
                    if (txtKind[ti] != 2) continue;
                    int ci2 = txtType[ti];
                    dbg2.append(" [C:").append(txtName[ti]).append('/').append(txtInst[ti])
                        .append("=>").append(ci2 < 0 || ci2 >= CAT_N ? "?" : CAT_KEY[ci2])
                        .append(ci2 >= 0 && ci2 < CAT_N && catOn[ci2] == 1 ? ":on" : ":off").append(']');
                    sh2++;
                }
                int[] kc = new int[5];
                String[] ks = new String[5];
                for (int ti = 0; ti < txtN; ti++) {
                    int kk = txtKind[ti];
                    if (kk >= 0 && kk < 5) {
                        kc[kk]++;
                        if (ks[kk] == null && txtName[ti] != null) ks[kk] = txtName[ti];
                    }
                }
                StringBuilder dbg3 = new StringBuilder(" [K");
                for (int kk = 0; kk < 5; kk++) dbg3.append(kk).append('=').append(kc[kk]).append('/').append(ks[kk]).append(' ');
                dbg3.append(']');
                System.out.println("[*] frame " + f + " fps=" + fpsNow + " tap=" + tapCount + " hidden=" + hidden + dbg3 + dbg);
            }
            Thread.sleep(Math.max(3, 1000 / Math.max(30, fpsSel)));
          } catch (Throwable th) {
              System.out.println("[!] frame " + f + " 异常(已吞掉继续画): " + th);
              try { Thread.sleep(50); } catch (Throwable t2) {}
          }
        }
    }
}
