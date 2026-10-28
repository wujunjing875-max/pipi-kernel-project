# -*- coding: utf-8 -*-
"""给 Ov50.java 注入自瞄模块（按钮/圈/贝塞尔触摸注入/速度参数）"""
import io, sys, os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src/Java/Ov50.java")
s = io.open(P, encoding="utf-8").read()
orig_len = len(s)
n = 0

def rep(old, new, tag):
    global s, n
    if old not in s:
        print("!! 锚点没找到:", tag); sys.exit(1)
    if s.count(old) != 1:
        print("!! 锚点不唯一(%d):" % s.count(old), tag); sys.exit(1)
    s = s.replace(old, new)
    n += 1
    print("  ok", tag)

# ---------- 1. 变量 ----------
rep(
"""    static float SX = 1.0f, SY = 1.0f;   // 屏幕坐标缩放：图层宽高 / 采集端投影基准 2414x1080""",
"""    static float SX = 1.0f, SY = 1.0f;   // 屏幕坐标缩放：图层宽高 / 采集端投影基准 2414x1080

    // ================= 自瞄 AIM =================
    static volatile boolean aimOn = false;                   // 自瞄总开关
    static volatile boolean aimCircleOn = true;              // 自瞄圈显隐
    static volatile float aimBtnX = 300f, aimBtnY = 1150f;   // 「自瞄开关」按钮中心（图层坐标）
    static volatile int aimBtnSize = 260;                    // 按钮大小
    static volatile int aimR = 700;                          // 自瞄圈半径
    static volatile int aimSpeed = 8;                        // 拉枪速度 1..20（小=软锁 大=强锁）
    static volatile int aimTouchSpeed = 12;                  // 触摸速度 1..20（贝塞尔步进快慢）
    static volatile boolean aimDrag = false;                 // 正在拖按钮
    static float aimDragDX = 0f, aimDragDY = 0f;
    static volatile long aimInjectUntil = 0;                 // 注入期间忽略自身事件，防误触面板
    static volatile boolean aimHolding = false;
    static volatile float aimHoldX = 0f, aimHoldY = 0f;
    static volatile int aimHitCount = 0;                     // 本帧圈内目标数
    static java.io.RandomAccessFile aimTp = null;            // /dev/input/event4 写句柄
    static final int AIM_TP_ID = 7777;                       // 注入用 tracking id""",
"vars")

# ---------- 2. 触摸注入 + 自瞄线程 + 绘制（插在 saveCfg 前）----------
AIM_BLOCK = r'''    // ============================================================
    // 自瞄：触摸注入（root 直写真实触摸屏 /dev/input/event4）
    // ============================================================
    static void tpOpen() {
        try {
            aimTp = new java.io.RandomAccessFile("/dev/input/event4", "rw");
            System.out.println("[AIM] tp open ok /dev/input/event4");
        } catch (Throwable t) {
            System.out.println("[!] AIM tp open fail: " + t);
        }
    }
    static void tpEv(int type, int code, int value) {
        if (aimTp == null) return;
        try {
            byte[] b = new byte[24];
            b[16] = (byte) type;  b[17] = (byte) (type >> 8);
            b[18] = (byte) code;  b[19] = (byte) (code >> 8);
            b[20] = (byte) value; b[21] = (byte) (value >> 8);
            b[22] = (byte) (value >> 16); b[23] = (byte) (value >> 24);
            aimTp.write(b);
        } catch (Throwable t) {}
    }
    static void tpDown(float sx, float sy) {
        aimInjectUntil = System.currentTimeMillis() + 400;
        tpEv(3, 47, 0);
        tpEv(3, 57, AIM_TP_ID);
        tpEv(3, 53, (int) (sy * 10f));
        tpEv(3, 54, (int) (sx * 10f));
        tpEv(1, 330, 1);
        tpEv(0, 0, 0);
    }
    static void tpMoveTo(float sx, float sy) {
        tpEv(3, 53, (int) (sy * 10f));
        tpEv(3, 54, (int) (sx * 10f));
        tpEv(0, 0, 0);
    }
    static void tpUp() {
        tpEv(3, 57, -1);
        tpEv(1, 330, 0);
        tpEv(0, 0, 0);
    }
    static boolean aimBtnHit(float sx, float sy) {
        float dx = sx - aimBtnX, dy = sy - aimBtnY;
        float h = aimBtnSize / 2f + 8f;
        return dx * dx + dy * dy <= h * h;
    }
    /** 三次缓动：起步快末端慢，最像人手 */
    static float aimEase(float t) {
        float u = 1f - t;
        return 1f - u * u * u;
    }
    /**
     * 单帧拉枪：保持按住，按贝塞尔轨迹把视角推 (mx,my)
     * 软锁 = 每帧挪一点、多帧收敛；强锁 = 一帧贴脸
     */
    static void aimStep(float mx, float my, int steps) {
        if (aimTp == null) tpOpen();
        if (!aimHolding) {
            aimHoldX = aimBtnX; aimHoldY = aimBtnY;
            tpDown(aimHoldX, aimHoldY);
            aimHolding = true;
        }
        float x0 = aimHoldX, y0 = aimHoldY;
        // 法线扰动 → 轨迹带弧度（拟人，抗直线判定）
        float len = (float) Math.hypot(mx, my);
        if (len < 1f) len = 1f;
        float nxx = -my / len, nyy = mx / len;
        float jit = Math.min(26f, len * 0.14f);
        float o1 = (float) ((Math.random() * 2 - 1) * jit);
        float o2 = (float) ((Math.random() * 2 - 1) * jit);
        float p1x = x0 + mx * 0.30f + nxx * o1, p1y = y0 + my * 0.30f + nyy * o1;
        float p2x = x0 + mx * 0.70f + nxx * o2, p2y = y0 + my * 0.70f + nyy * o2;
        float ex = x0 + mx, ey = y0 + my;
        for (int i = 1; i <= steps; i++) {
            float t = aimEase((float) i / steps);
            float u = 1f - t;
            float bx = u*u*u*x0 + 3*u*u*t*p1x + 3*u*t*t*p2x + t*t*t*ex;
            float by = u*u*u*y0 + 3*u*u*t*p1y + 3*u*t*t*p2y + t*t*t*ey;
            tpMoveTo(bx, by);
            try { Thread.sleep(1); } catch (InterruptedException ignored) {}
        }
        aimHoldX = ex; aimHoldY = ey;
        // 漂太远就重新落点，避免手指"飞出屏幕"
        if (Math.abs(aimHoldX - aimBtnX) > 280f || Math.abs(aimHoldY - aimBtnY) > 280f) {
            tpUp(); aimHolding = false;
        }
    }
    static void aimRelease() {
        if (aimHolding) { tpUp(); aimHolding = false; }
    }

    /** 自瞄主循环：选目标 -> 算偏差 -> 拉枪 */
    static void aimThread() {
        new Thread(new Runnable() {
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(aimSpeed > 14 ? 8 : 14);
                        if (!aimOn) { aimRelease(); aimHitCount = 0; continue; }
                        if (aimTp == null) tpOpen();
                        int best = -1; float bestScore = 1e9f;
                        int cnt = 0;
                        int total = txtN < 128 ? txtN : 128;
                        for (int i = 0; i < total; i++) {
                            int k = txtKind[i];
                            if (k != 0 && k != 1 && k != 4) continue;     // 只锁真人/人机/人机玩家
                            float dx = txtX[i] - aimBtnX, dy = txtY[i] - aimBtnY;
                            float d = (float) Math.hypot(dx, dy);
                            if (d > aimR) continue;                      // 只打圈内
                            cnt++;
                            float score = d + txtDm[i] * 0.35f;           // 近 + 近距优先
                            if (score < bestScore) { bestScore = score; best = i; }
                        }
                        aimHitCount = cnt;
                        if (best < 0) { aimRelease(); continue; }
                        float dx = txtX[best] - aimBtnX, dy = txtY[best] - aimBtnY;
                        float len = (float) Math.hypot(dx, dy);
                        if (len < 9f) continue;                          // 已在准星上
                        // 拉枪速度：1..20 -> 收敛比例；<=5 走软锁逐帧小步
                        float k = aimSpeed / 20f;
                        float frac;
                        if (aimSpeed <= 5)      frac = 0.10f + k * 0.08f;
                        else if (aimSpeed <= 12) frac = 0.10f + k * k * 0.55f;
                        else                     frac = 0.35f + (k - 0.6f) * 1.45f;
                        if (frac > 0.95f) frac = 0.95f;
                        float mx = dx * frac, my = dy * frac;
                        // 触摸速度：1..20 -> 步数 14..3（步多=慢而稳，步少=快而硬）
                        int steps = 14 - (aimTouchSpeed / 2);
                        if (steps < 3) steps = 3;
                        aimStep(mx, my, steps);
                    } catch (Throwable t) {
                        try { aimRelease(); } catch (Throwable t2) {}
                    }
                }
            }
        }).start();
    }

    /** 自瞄 UI：圈 + 「自瞄开关」按钮（面板隐藏也常显） */
    static void drawAim(Object c) throws Exception {
        float cx0 = aimBtnX, cy0 = aimBtnY;
        if (aimCircleOn) {
            int col = aimOn ? 0xAA00E676 : 0x7700D4FF;
            int seg = 64;
            float prx = cx0 + aimR, pry = cy0;
            for (int i = 1; i <= seg; i++) {
                double an = i * 2.0 * Math.PI / seg;
                float nx2 = (float) (cx0 + Math.cos(an) * aimR);
                float ny2 = (float) (cy0 + Math.sin(an) * aimR);
                line(c, prx, pry, nx2, ny2, 3f, col);
                prx = nx2; pry = ny2;
            }
            line(c, cx0 - 26, cy0, cx0 + 26, cy0, 2f, col);
            line(c, cx0, cy0 - 26, cx0, cy0 + 26, 2f, col);
        }
        int bs = aimBtnSize, half = bs / 2;
        int bx = (int) (cx0 - half), by = (int) (cy0 - half);
        int bg = aimOn ? 0xE600C853 : 0xD91B1F24;
        int fg = aimOn ? 0xFF06281A : 0xFFAEB9C2;
        rect(c, bx, by, bx + bs, by + bs, bg);
        rect(c, bx, by, bx + bs, by + 4, aimOn ? 0xFFB9F6CA : 0xFF3A4048);
        rect(c, bx, by + bs - 3, bx + bs, by + bs, 0x55000000);
        if (tfCN != null) {
            textCN(c, aimOn ? "锁定" : "自瞄", bx + bs * 0.17f, by + bs * 0.62f, bs * 0.30f, fg);
        } else if (glyphBmp == null) {
            gtext(c, aimOn ? "ON" : "AIM", bx + (int) (bs * 0.22f), by + (int) (bs * 0.40f), fg);
        } else {
            gtext(c, aimOn ? "ON" : "AI", bx + (int) (bs * 0.30f), by + (int) (bs * 0.40f), fg);
        }
        if (aimOn && aimHitCount > 0) {
            text(c, "" + aimHitCount, bx + bs - 24, by + 8, 3, 0xFFFFD54F);
        }
    }

    static void saveCfg() {'''
rep("    static void saveCfg() {", AIM_BLOCK, "aim block")

# ---------- 3. inputThread：忽略自己注入的事件 ----------
rep(
"""                            int value = (buf[20] & 0xff) | ((buf[21] & 0xff) << 8) | ((buf[22] & 0xff) << 16) | ((buf[23] & 0xff) << 24);""",
"""                            int value = (buf[20] & 0xff) | ((buf[21] & 0xff) << 8) | ((buf[22] & 0xff) << 16) | ((buf[23] & 0xff) << 24);
                            // 自瞄注入的事件会回环到本读取器：注入窗口内的 TOUCH/TRACKING 一律忽略，
                            // 否则自瞄拉枪会被当成用户点击 → 误触面板开关
                            if (System.currentTimeMillis() < aimInjectUntil
                                    && ((type == 1 && code == 330) || (type == 3 && code == 57)))
                                continue;""",
"input filter")

# ---------- 4. beginGesture：按住自瞄按钮 = 拖位置 ----------
rep(
"""        scrolling = false; dragging = false;
        if (calStep < 2) return;""",
"""        scrolling = false; dragging = false; aimDrag = false;
        // 「自瞄开关」按钮优先：按住它 = 调位置（圈跟着走，避开游戏内按钮）
        if (aimBtnHit(sx, sy)) {
            aimDrag = true;
            aimDragDX = sx - aimBtnX; aimDragDY = sy - aimBtnY;
            tapMoved = true;
            System.out.println("[AIM] drag start");
            return;
        }
        if (calStep < 2) return;""",
"beginGesture aim drag")

# ---------- 5. 拖动分支 ----------
rep(
"""                            if (down == 1 && gestureDone && (scrolling || dragging) && ax >= 0 && ay >= 0) {
                                int[] sp2 = toScreen(ax, ay);
                                int sx = sp2[0], sy = sp2[1];
                                if (scrolling) {""",
"""                            if (down == 1 && gestureDone && (scrolling || dragging || aimDrag) && ax >= 0 && ay >= 0) {
                                int[] sp2 = toScreen(ax, ay);
                                int sx = sp2[0], sy = sp2[1];
                                if (aimDrag) {
                                    float nbx = sx - aimDragDX, nby = sy - aimDragDY;
                                    float half2 = aimBtnSize / 2f;
                                    if (nbx < half2) nbx = half2;
                                    if (nbx > W - half2) nbx = W - half2;
                                    if (nby < half2) nby = half2;
                                    if (nby > H - half2) nby = H - half2;
                                    aimBtnX = nbx; aimBtnY = nby;
                                    tapMoved = true;
                                } else if (scrolling) {""",
"drag move branch")

# ---------- 6. onTap：点按钮开关 ----------
rep(
"""        float lx = x - PX, ly = y - PY;
        if (wasMove) return;                       // 滑动过就不触发点击""",
"""        float lx = x - PX, ly = y - PY;
        if (wasMove) return;                       // 滑动过就不触发点击
        // 「自瞄开关」按钮：单击 开/关
        if (aimBtnHit(x, y)) {
            aimOn = !aimOn;
            if (!aimOn) aimRelease();
            saveCfg();
            System.out.println("[AIM] toggled -> " + aimOn);
            return;
        }""",
"onTap aim toggle")

# ---------- 7. loadCfg ----------
rep(
"""                if (k.startsWith("cat_")) {""",
"""                if (k.equals("aim")) { aimOn = v.trim().equals("1"); if (!aimOn) aimRelease(); continue; }
                if (k.equals("aimcircle")) { aimCircleOn = v.trim().equals("1"); continue; }
                if (k.equals("aimpos")) {
                    String[] ap = v.split(",");
                    try {
                        if (ap.length >= 2) { aimBtnX = Float.parseFloat(ap[0].trim()); aimBtnY = Float.parseFloat(ap[1].trim()); }
                    } catch (Throwable t5) { }
                    continue;
                }
                if (k.equals("aimsize")) { try { aimBtnSize = Integer.parseInt(v.trim()); } catch (Throwable t5) { } continue; }
                if (k.equals("aimr")) { try { aimR = Integer.parseInt(v.trim()); } catch (Throwable t5) { } continue; }
                if (k.equals("aimspeed")) { try { aimSpeed = Integer.parseInt(v.trim()); } catch (Throwable t5) { } continue; }
                if (k.equals("aimtouch")) { try { aimTouchSpeed = Integer.parseInt(v.trim()); } catch (Throwable t5) { } continue; }
                if (k.startsWith("cat_")) {""",
"loadCfg aim")

# ---------- 8. saveCfg 写回 ----------
rep(
"""            sb.append("radpos=").append(radCx).append(',').append(radCy).append(',').append(radR).append('\\n');
            java.io.FileWriter fw = new java.io.FileWriter(DIR + "ov_cfg.txt");""",
"""            sb.append("radpos=").append(radCx).append(',').append(radCy).append(',').append(radR).append('\\n');
            // 自瞄：必须写回，App 面板和绘制端共用这一份配置
            sb.append("aim=").append(aimOn ? 1 : 0).append('\\n');
            sb.append("aimcircle=").append(aimCircleOn ? 1 : 0).append('\\n');
            sb.append("aimpos=").append((int) aimBtnX).append(',').append((int) aimBtnY).append('\\n');
            sb.append("aimsize=").append(aimBtnSize).append('\\n');
            sb.append("aimr=").append(aimR).append('\\n');
            sb.append("aimspeed=").append(aimSpeed).append('\\n');
            sb.append("aimtouch=").append(aimTouchSpeed).append('\\n');
            java.io.FileWriter fw = new java.io.FileWriter(DIR + "ov_cfg.txt");""",
"saveCfg aim")

# ---------- 9. 每帧绘制 ----------
rep(
"""            if (labelOn == 1) drawLabels(c);""",
"""            if (labelOn == 1) drawLabels(c);
            try { drawAim(c); } catch (Throwable taim) { System.out.println("[!] aim draw: " + taim); }""",
"draw call")

# ---------- 10. 启动线程 ----------
rep(
"""        inputThread();""",
"""        inputThread();
        tpOpen();
        aimThread();""",
"start threads")

io.open(P, "w", encoding="utf-8", newline="\n").write(s)
print("\n补丁完成 %d 处，%d -> %d bytes" % (n, orig_len, len(s)))
