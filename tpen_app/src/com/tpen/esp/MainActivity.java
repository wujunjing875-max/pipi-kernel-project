package com.tpen.esp;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class MainActivity extends Activity {

    static final String DIR = "/data/local/tmp/.tpen";
    String local;                 // App 私有目录：/data/data/<pkg>/files/tpen
    int W = 1920, H = 1080;       // 自动检测到的横屏分辨率
    boolean running = false;

    // 开关状态（默认全开，跟平板调好的一致）
    boolean[] kind = { true, true, true, true, true };   // 玩家 人机 容器 物资 人机玩家
    boolean bone = true, lab = true, abar = true;
    boolean[] dec = { true, false, false, false };       // 解密1 开（解密4 会飘，默认关）
    int fpsSel = 144;
    int priceSel = 0;
    boolean creator = false;                              // 创作者模式：默认关，悬浮窗隐藏


    // 雷达（拿采集端的实体世界坐标画小地图）
    static final int[] RAD_RANGES = { 30, 40, 60, 80, 100 };
    static final int[][] RAD_POS = { { 360, 1980 }, { 360, 360 }, { 3030, 1980 }, { 3030, 360 } };
    static final String[] RAD_POS_CN = { "左下", "左上", "右下", "右上" };
    static final int RAD_R = 250;      // 雷达半径（设备像素）
    boolean radar = true;
    boolean radRot = true;             // true=前方朝上
    boolean radBots = true;
    boolean radLoot = false;
    int radRangeSel = 2;               // 默认 60m
    int radPosSel = 0;                 // 默认左下
    android.content.SharedPreferences prefs;

    // ===== 内核环境（第二层：内核卡）=====
    EnvLicense envLic;
    TextView tvEnvStatus, tvKernelLic;
    EditText etKernelCard;
    boolean envRepoChecked = false;

    TextView tvStatus, tvRes, tvLic;
    Button btnMain;
    License lic;
    EditText etCard;
    Button btnAct;
    LinearLayout licRow;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        local = getFilesDir().getAbsolutePath() + "/tpen";
        new File(local).mkdirs();
        prefs = getSharedPreferences("tpen", MODE_PRIVATE);
        envLic = new EnvLicense(this);
        creator = prefs.getBoolean("creator", false);

        ScrollView sv = new ScrollView(this);
        sv.setBackgroundColor(Color.parseColor("#F2F2F7"));
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(16);
        root.setPadding(pad, dp(24), pad, dp(24));
        sv.addView(root);

        // 标题
        TextView title = new TextView(this);
        title.setText("TPEN ESP");
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
        title.setTextColor(Color.BLACK);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, dp(6));
        root.addView(title);

        TextView sub = new TextView(this);
        sub.setText("内核启动器 · 需 Root 授权");
        sub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        sub.setTextColor(Color.parseColor("#8A8A8E"));
        sub.setGravity(Gravity.CENTER);
        sub.setPadding(0, 0, 0, dp(16));
        root.addView(sub);

        // 状态卡片
        tvStatus = new TextView(this);
        tvStatus.setText("状态：未启动");
        tvStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvStatus.setTextColor(Color.parseColor("#1C1C1E"));
        tvStatus.setPadding(dp(14), dp(12), dp(14), dp(12));
        tvStatus.setBackground(card(Color.WHITE));
        root.addView(tvStatus);
        addGap(root, 10);

        tvRes = new TextView(this);
        tvRes.setText("分辨率：检测中…");
        tvRes.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvRes.setTextColor(Color.parseColor("#8A8A8E"));
        tvRes.setPadding(dp(14), dp(8), dp(14), dp(8));
        tvRes.setBackground(card(Color.WHITE));
        root.addView(tvRes);
        addGap(root, 10);

        // 授权卡片
        root.addView(groupTitle("卡密授权（环境 / 内核一 / 内核二 同机通用）"));
        LinearLayout g0 = group();
        tvLic = new TextView(this);
        tvLic.setText("授权：检测中…");
        tvLic.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvLic.setTextColor(Color.parseColor("#1C1C1E"));
        tvLic.setPadding(dp(8), dp(8), dp(8), dp(8));
        g0.addView(tvLic);

        licRow = new LinearLayout(this);
        licRow.setOrientation(LinearLayout.HORIZONTAL);
        licRow.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(52)));
        licRow.setGravity(Gravity.CENTER_VERTICAL);
        etCard = new EditText(this);
        etCard.setHint("输入卡密");
        etCard.setSingleLine(true);
        etCard.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        etCard.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        btnAct = new Button(this);
        btnAct.setText("激活");
        btnAct.setMinWidth(0);
        btnAct.setMinimumWidth(0);
        btnAct.setPadding(dp(18), dp(4), dp(18), dp(4));
        btnAct.setTextColor(Color.WHITE);
        btnAct.setBackground(card(Color.parseColor("#34C759")));
        btnAct.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { doActivate(); }
        });
        licRow.addView(etCard);
        licRow.addView(btnAct);
        g0.addView(licRow);
        root.addView(g0);
        addGap(root, 16);

        // 创作者模式（悬浮窗显隐总开关）
        root.addView(groupTitle("创作者模式"));
        LinearLayout gc = group();
        gc.addView(row("显示菜单面板", creator, new OnToggle() {
            public void on(boolean v) { creator = v; pushCreator(); }
        }));
        TextView cNote = new TextView(this);
        cNote.setText("只控制「菜单面板」这一块悬浮窗的显示。\n"
                + "关闭：面板收起，方框 / 射线 / 骨骼 / 文字标签 / 解密等所有绘制效果一个不少，照常工作。\n"
                + "打开：面板立刻出现（约 0.1 秒），可现场改开关、调参数。");
        cNote.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        cNote.setTextColor(Color.parseColor("#8A8A8E"));
        cNote.setPadding(dp(8), dp(2), dp(8), dp(10));
        cNote.setLineSpacing(0, 1.25f);
        gc.addView(cNote);
        root.addView(gc);
        addGap(root, 16);

        // 一键启动按钮
        btnMain = new Button(this);
        btnMain.setText("一键启动");
        btnMain.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        btnMain.setTextColor(Color.WHITE);
        btnMain.setBackground(card(Color.parseColor("#007AFF")));
        btnMain.setHeight(dp(52));
        btnMain.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (running) doStop(); else doStart();
            }
        });
        root.addView(btnMain);
        addGap(root, 18);

        // 显示项
        final String[] kName = { "玩家（真人）", "人机（AI）", "容器（盒子）", "物资（地面物品）", "人机玩家" };
        root.addView(groupTitle("显示项目"));
        LinearLayout g1 = group();
        for (int i = 0; i < 5; i++) {
            final int idx = i;
            g1.addView(row(kName[i], kind[i], new OnToggle() {
                public void on(boolean v) { kind[idx] = v; pushCfg(); }
            }));
        }
        root.addView(g1);
        addGap(root, 14);

        // 功能
        root.addView(groupTitle("绘制功能"));
        LinearLayout g2 = group();
        g2.addView(row("骨骼", bone, new OnToggle() { public void on(boolean v) { bone = v; pushCfg(); } }));
        g2.addView(row("文字标签", lab, new OnToggle() { public void on(boolean v) { lab = v; pushCfg(); } }));
        g2.addView(row("护甲等级条", abar, new OnToggle() { public void on(boolean v) { abar = v; pushCfg(); } }));
        root.addView(g2);
        addGap(root, 14);

        // 雷达
        root.addView(groupTitle("雷达"));
        LinearLayout gRad = group();
        gRad.addView(row("开启雷达", radar, new OnToggle() { public void on(boolean v) { radar = v; pushCfg(); } }));
        final TextView tvRP = new TextView(this);
        gRad.addView(stepRow("雷达位置", tvRP, new Runnable() {
            public void run() { radPosSel = (radPosSel + 1) % RAD_POS.length; tvRP.setText(RAD_POS_CN[radPosSel]); pushCfg(); }
        }));
        tvRP.setText(RAD_POS_CN[radPosSel]);
        final TextView tvRR = new TextView(this);
        gRad.addView(stepRow("雷达半径", tvRR, new Runnable() {
            public void run() { radRangeSel = (radRangeSel + 1) % RAD_RANGES.length; tvRR.setText(RAD_RANGES[radRangeSel] + " m"); pushCfg(); }
        }));
        tvRR.setText(RAD_RANGES[radRangeSel] + " m");
        final TextView tvRot = new TextView(this);
        gRad.addView(stepRow("雷达朝向", tvRot, new Runnable() {
            public void run() { radRot = !radRot; tvRot.setText(radRot ? "前方朝上" : "北朝上"); pushCfg(); }
        }));
        tvRot.setText(radRot ? "前方朝上" : "北朝上");
        gRad.addView(row("雷达显示人机", radBots, new OnToggle() { public void on(boolean v) { radBots = v; pushCfg(); } }));
        gRad.addView(row("雷达显示物资", radLoot, new OnToggle() { public void on(boolean v) { radLoot = v; pushCfg(); } }));
        root.addView(gRad);
        addGap(root, 14);

        // ===== 内核环境（第二层卡密）=====
        root.addView(groupTitle("内核环境（需内核卡，第二层）"));
        LinearLayout gKer = group();

        tvEnvStatus = new TextView(this);
        tvEnvStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvEnvStatus.setTextColor(Color.parseColor("#8A8A8E"));
        tvEnvStatus.setPadding(dp(12), dp(10), dp(12), dp(6));
        tvEnvStatus.setText("环境引擎状态：检测中…");
        gKer.addView(tvEnvStatus);

        tvKernelLic = new TextView(this);
        tvKernelLic.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvKernelLic.setTextColor(Color.parseColor("#FF6B6B"));
        tvKernelLic.setPadding(dp(12), 0, dp(12), dp(6));
        tvKernelLic.setText("内核卡：未激活");
        gKer.addView(tvKernelLic);

        // 内核卡输入 + 激活
        LinearLayout kRow = new LinearLayout(this);
        kRow.setOrientation(LinearLayout.HORIZONTAL);
        kRow.setGravity(Gravity.CENTER_VERTICAL);
        kRow.setPadding(dp(12), dp(4), dp(12), dp(8));
        etKernelCard = new EditText(this);
        etKernelCard.setHint("MF-XXXX-XXXX-XXXX-XXXX");
        etKernelCard.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        etKernelCard.setSingleLine(true);
        kRow.addView(etKernelCard, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        Button btnKAct = new Button(this);
        btnKAct.setText("激活内核卡");
        btnKAct.setAllCaps(false);
        btnKAct.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnKAct.setTextColor(Color.WHITE);
        btnKAct.setBackground(card(Color.parseColor("#7B1FA2")));
        btnKAct.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { doActivateKernel(); }
        });
        LinearLayout.LayoutParams klp = new LinearLayout.LayoutParams(dp(104), dp(40));
        klp.leftMargin = dp(6);
        kRow.addView(btnKAct, klp);
        gKer.addView(kRow);

        // 内核启停（必须先通过内核卡）
        LinearLayout eRow = new LinearLayout(this);
        eRow.setOrientation(LinearLayout.HORIZONTAL);
        eRow.setPadding(dp(12), dp(2), dp(12), dp(12));
        Button btnEnvOn = new Button(this);
        btnEnvOn.setText("打开环境软件");
        btnEnvOn.setAllCaps(false);
        btnEnvOn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnEnvOn.setTextColor(Color.WHITE);
        btnEnvOn.setBackground(card(Color.parseColor("#00695C")));
        btnEnvOn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { envStart(); }
        });
        eRow.addView(btnEnvOn, new LinearLayout.LayoutParams(0, dp(44), 1f));
        Button btnEnvOff = new Button(this);
        btnEnvOff.setText("刷新状态");
        btnEnvOff.setAllCaps(false);
        btnEnvOff.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnEnvOff.setTextColor(Color.WHITE);
        btnEnvOff.setBackground(card(Color.parseColor("#5D4037")));
        btnEnvOff.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { envStop(); }
        });
        LinearLayout.LayoutParams olp = new LinearLayout.LayoutParams(dp(80), dp(44));
        olp.leftMargin = dp(8);
        eRow.addView(btnEnvOff, olp);
        gKer.addView(eRow);

        TextView kNote = new TextView(this);
        kNote.setText("内核卡与普通卡密是两套：普通卡密只解锁绘制，内核卡才解锁内核板块。一码一设备，首次激活即绑本机指纹。\n本板块只读环境状态、不改环境软件的任何设置；环境引擎的启停/方案切换请在本体内操作。\n配置每次退出自动保存，换设备不用重调。");
        kNote.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        kNote.setTextColor(Color.parseColor("#8A8A8E"));
        kNote.setPadding(dp(8), 0, dp(8), dp(10));
        kNote.setLineSpacing(0, 1.25f);
        gKer.addView(kNote);
        root.addView(gKer);
        addGap(root, 14);

        // 解密
        root.addView(groupTitle("坐标解密（只开一个）"));
        LinearLayout g3 = group();
        String[] dName = { "解密 1（推荐）", "解密 2", "解密 3", "解密 4" };
        for (int i = 0; i < 4; i++) {
            final int idx = i;
            g3.addView(row(dName[i], dec[i], new OnToggle() {
                public void on(boolean v) {
                    if (v) for (int j = 0; j < 4; j++) dec[j] = (j == idx);
                    else dec[idx] = false;
                    refreshDec();
                    pushCfg();
                }
            }));
        }
        root.addView(g3);
        addGap(root, 14);

        // 帧率 / 价格
        root.addView(groupTitle("参数"));
        LinearLayout g4 = group();
        final TextView tvFps = new TextView(this);
        g4.addView(stepRow("绘制帧率", tvFps, new Runnable() {
            public void run() {
                fpsSel = (fpsSel == 60) ? 90 : (fpsSel == 90 ? 120 : (fpsSel == 120 ? 144 : 60));
                tvFps.setText(fpsSel + " FPS");
                pushCfg();
            }
        }));
        tvFps.setText(fpsSel + " FPS");
        final TextView tvPrice = new TextView(this);
        g4.addView(stepRow("物资价格档", tvPrice, new Runnable() {
            public void run() {
                priceSel = (priceSel + 1) % 5;
                tvPrice.setText("档位 " + priceSel);
                pushCfg();
            }
        }));
        tvPrice.setText("档位 " + priceSel);
        root.addView(g4);
        addGap(root, 18);

        // 清理
        Button btnClean = new Button(this);
        btnClean.setText("清理工作目录");
        btnClean.setTextColor(Color.parseColor("#FF3B30"));
        btnClean.setBackground(card(Color.WHITE));
        btnClean.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { doClean(); }
        });
        root.addView(btnClean);

        setContentView(sv);
        refreshKernelLic();
        refreshEnvStatus();

        detectRes();
        checkRoot();
        extractAssets();
        lic = new License(this);
        checkLic();
    }

    // ---------- 卡密 ----------
    void checkLic() {
        btnMain.setEnabled(false);
        btnAct.setEnabled(false);
        tvLic.setText("授权：正在检测本机凭证…");
        new Thread(new Runnable() {
            public void run() {
                final boolean found = lic.scan();
                String hb = "";
                if (found) hb = lic.heartbeat();
                final String msg = hb;
                runOnUiThread(new Runnable() {
                    public void run() {
                        btnAct.setEnabled(true);
                        if (lic.active()) {
                            licRow.setVisibility(View.GONE);
                            tvLic.setText("授权：已激活 ✓\n设备码 " + shortHw(lic.hwid())
                                    + " · 到期 " + lic.expireText()
                                    + "\n套餐 " + lic.member()
                                    + (lic.srcPath.length() > 0 ? "\n凭证来自 " + lic.srcPath : "")
                                    + (msg.length() > 0 ? "\n复核：" + msg : ""));
                            btnMain.setEnabled(true);
                        } else {
                            licRow.setVisibility(View.VISIBLE);
                            tvLic.setText("授权：未激活 ✗\n设备码 " + shortHw(lic.hwid())
                                    + "\n请先输入卡密激活（同机已激活可直接用）"
                                    + (msg.length() > 0 ? "\n" + msg : ""));
                            btnMain.setEnabled(false);
                        }
                    }
                });
            }
        }).start();
    }

    String shortHw(String h) {
        if (h == null || h.length() < 8) return h == null ? "-" : h;
        return h.substring(0, 8) + "…";
    }

    void doActivate() {
        final String c = etCard.getText().toString();
        if (c.trim().length() == 0) {
            Toast.makeText(this, "请输入卡密", Toast.LENGTH_SHORT).show();
            return;
        }
        btnAct.setEnabled(false);
        tvLic.setText("授权：正在校验卡密…");
        new Thread(new Runnable() {
            public void run() {
                final String r = lic.activate(c);
                runOnUiThread(new Runnable() {
                    public void run() {
                        btnAct.setEnabled(true);
                        if (r.startsWith("OK")) {
                            licRow.setVisibility(View.GONE);
                            tvLic.setText("授权：已激活 ✓\n设备码 " + shortHw(lic.hwid())
                                    + " · 到期 " + lic.expireText()
                                    + "\n套餐 " + lic.member() + "\n" + r.substring(2));
                            btnMain.setEnabled(true);
                            Toast.makeText(MainActivity.this, "激活成功", Toast.LENGTH_SHORT).show();
                        } else {
                            tvLic.setText("授权：未激活 ✗\n" + r);
                            btnMain.setEnabled(false);
                        }
                    }
                });
            }
        }).start();
    }

    interface OnToggle { void on(boolean v); }

    // ---------- UI 辅助 ----------
    int dp(int v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    GradientDrawable card(int color) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(12));
        return d;
    }

    void addGap(LinearLayout p, int h) {
        View v = new View(this);
        v.setLayoutParams(new LinearLayout.LayoutParams(1, dp(h)));
        p.addView(v);
    }

    TextView groupTitle(String t) {
        TextView tv = new TextView(this);
        tv.setText(t);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tv.setTextColor(Color.parseColor("#8A8A8E"));
        tv.setPadding(dp(6), 0, 0, dp(6));
        return tv;
    }

    LinearLayout group() {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setBackground(card(Color.WHITE));
        l.setPadding(dp(8), dp(4), dp(8), dp(4));
        return l;
    }

    View row(String name, boolean init, final OnToggle cb) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.HORIZONTAL);
        l.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(48)));
        l.setGravity(Gravity.CENTER_VERTICAL);
        TextView tv = new TextView(this);
        tv.setText(name);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tv.setTextColor(Color.BLACK);
        tv.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        final Switch sw = new Switch(this);
        sw.setChecked(init);
        sw.setOnCheckedChangeListener(new android.widget.CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(android.widget.CompoundButton b, boolean v) { cb.on(v); }
        });
        l.addView(tv);
        l.addView(sw);
        return l;
    }

    View stepRow(String name, final TextView val, final Runnable cb) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.HORIZONTAL);
        l.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(48)));
        l.setGravity(Gravity.CENTER_VERTICAL);
        TextView tv = new TextView(this);
        tv.setText(name);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tv.setTextColor(Color.BLACK);
        tv.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        val.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        val.setTextColor(Color.parseColor("#007AFF"));
        val.setPadding(0, 0, dp(12), 0);
        Button b = new Button(this);
        b.setText("切换");
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setPadding(dp(14), dp(4), dp(14), dp(4));
        b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { cb.run(); }
        });
        l.addView(tv);
        l.addView(val);
        l.addView(b);
        return l;
    }

    void refreshDec() {   // 互斥开关：重绘解密分组
        recreate();
    }

    // ---------- 逻辑 ----------
    void detectRes() {
        DisplayMetrics real = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(real);
        int rw = real.widthPixels, rh = real.heightPixels;
        W = Math.max(rw, rh);
        H = Math.min(rw, rh);         // 横屏：长边为宽
        tvRes.setText("分辨率：" + rw + " x " + rh + "  →  绘制层 " + W + " x " + H);
    }

    void checkRoot() {
        new Thread(new Runnable() {
            public void run() {
                final String out = su("id");
                runOnUiThread(new Runnable() {
                    public void run() {
                        if (out.contains("uid=0")) {
                            tvStatus.setText("状态：Root 已授权 ✓\n未启动内核");
                        } else {
                            tvStatus.setText("状态：未获得 Root 授权 ✗\n请在 Root 管理器中授予本 App");
                            btnMain.setEnabled(false);
                        }
                    }
                });
            }
        }).start();
    }

    void extractAssets() {
        try {
            AssetManager am = getAssets();
            String[] list = am.list("");
            if (list != null) for (String f : list) {
                try {                       // 单个文件失败不拖累其他文件
                    InputStream in = am.open(f);
                    OutputStream os = new FileOutputStream(local + "/" + f);
                    byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                    in.close();
                    os.close();
                } catch (Throwable t2) {
                    android.util.Log.e("TPEN", "extract fail " + f + " : " + t2);
                }
            }
        } catch (Throwable t) {
            android.util.Log.e("TPEN", "extract error " + t);
        }
    }

    void doStart() {
        if (lic == null || !lic.active()) {
            Toast.makeText(this, "请先激活卡密", Toast.LENGTH_SHORT).show();
            checkLic();
            return;
        }
        btnMain.setEnabled(false);
        tvStatus.setText("状态：正在刷入驱动…");
        new Thread(new Runnable() {
            public void run() {
                // 1) 生成启动脚本 + 释放到统一工作目录
                write(local + "/run_col.sh", colScript());
                write(local + "/run_ov.sh", ovScript());
                su("mkdir -p " + DIR + " && cp -f " + local + "/* " + DIR + "/ "
                        + "&& chmod 755 " + DIR + "/p " + DIR + "/drv.sh "
                        + DIR + "/run_col.sh " + DIR + "/run_ov.sh "
                        + "&& chmod 644 " + DIR + "/*.dex " + DIR + "/*.png " + DIR + "/*.txt");
                // 2) 选驱动（按内核版本）
                String kr = su("uname -r").trim();
                String drv = pickDrv(kr);
                su("cp -f " + DIR + "/" + drv + " " + DIR + "/drv.sh && chmod 755 " + DIR + "/drv.sh");
                runOnUiThread(new Runnable() {
                    public void run() { tvStatus.setText("状态：驱动 " + "刷入中…"); }
                });
                final String drvOut = su("sh " + DIR + "/drv.sh 2>&1 | tail -3");
                // 3) 起采集端 + 绘制端（脚本方式，避免 su 内嵌套引号失效）
                pushCreator();          // 先落创作模式开关，绘制端启动时直接按它决定显隐
                su("sh " + DIR + "/run_col.sh");
                su("sh " + DIR + "/run_ov.sh");
                sleep(2000);
                pushCfg();
                pushCreator();
                final String st = su("ps -A | grep -c app_process");
                runOnUiThread(new Runnable() {
                    public void run() {
                        running = true;
                        btnMain.setText("停止运行");
                        btnMain.setBackground(card(Color.parseColor("#FF3B30")));
                        btnMain.setEnabled(true);
                        tvStatus.setText("状态：内核已启动 ✓\n" + drvOut.trim()
                                + "\n绘制层 " + W + "x" + H + " · 绘制进程 " + st.trim());
                    }
                });
            }
        }).start();
    }

    void doStop() {
        btnMain.setEnabled(false);
        new Thread(new Runnable() {
            public void run() {
                su("pkill -9 -f Ov50; "
                        + "for d in /proc/[0-9]*; do e=$(readlink $d/exe 2>/dev/null); "
                        + "case \"$e\" in */.tpen/p*) kill -9 ${d#/proc/} 2>/dev/null;; esac; done; "
                        + "rmmod paradise 2>/dev/null; rm -f /data/local/tmp/esp.txt; echo stopped");
                runOnUiThread(new Runnable() {
                    public void run() {
                        running = false;
                        btnMain.setText("一键启动");
                        btnMain.setBackground(card(Color.parseColor("#007AFF")));
                        btnMain.setEnabled(true);
                        tvStatus.setText("状态：已停止");
                    }
                });
            }
        }).start();
    }

    void doClean() {
        new Thread(new Runnable() {
            public void run() {
                su("pkill -9 -f Ov50; rm -rf " + DIR + "; rm -f /data/local/tmp/esp.txt; echo cleaned");
                runOnUiThread(new Runnable() {
                    public void run() {
                        running = false;
                        btnMain.setText("一键启动");
                        btnMain.setBackground(card(Color.parseColor("#007AFF")));
                        tvStatus.setText("状态：工作目录已清理");
                        Toast.makeText(MainActivity.this, "已清理 " + DIR, Toast.LENGTH_SHORT).show();
                    }
                });
            }
        }).start();
    }

    String pickDrv(String kr) {
        String[] p = kr.split("[.\\-]");
        String key = "";
        if (p.length >= 2) key = p[0] + p[1];
        String[] have = { "510", "515", "61", "612", "618", "66" };
        for (String h : have) if (h.equals(key)) return "drv_" + h + ".sh";
        return "drv_66.sh";       // 兜底
    }

    String colScript() {
        return "#!/system/bin/sh\n"
                + "D=" + DIR + "\n"
                + "for d in /proc/[0-9]*; do\n"
                + "  e=$(readlink $d/exe 2>/dev/null)\n"
                + "  case \"$e\" in */.tpen/p*) kill -9 ${d#/proc/} 2>/dev/null;; esac\n"
                + "done\n"
                + "sleep 1\n"
                + "cd $D\n"
                + "setsid sh -c \"exec $D/p > $D/p.log 2>&1\" </dev/null >/dev/null 2>&1 &\n"
                + "echo col_started\n";
    }

    String ovScript() {
        return "#!/system/bin/sh\n"
                + "D=" + DIR + "\n"
                + "pkill -9 -f Ov50 2>/dev/null\n"
                + "sleep 1\n"
                + "cd $D\n"
                + "setsid sh -c \"CLASSPATH=$D/ov.dex exec app_process $D Ov50 "
                + W + " " + H + " 0 0 > $D/o.log 2>&1\" </dev/null >/dev/null 2>&1 &\n"
                + "sleep 4\n"
                + "echo ov_started\n";
    }

    void write(String path, String content) {
        try {
            FileOutputStream os = new FileOutputStream(path);
            os.write(content.getBytes("UTF-8"));
            os.close();
        } catch (Throwable t) {
            android.util.Log.e("TPEN", "write fail " + path + " : " + t);
        }
    }



    // ============ 内核环境：第二层卡密 ============

    void doActivateKernel() {
        final String cardTxt = etKernelCard.getText().toString().trim();
        if (cardTxt.length() < 10) {
            Toast.makeText(this, "请输入内核卡（MF-XXXX-XXXX-XXXX-XXXX）", Toast.LENGTH_SHORT).show();
            return;
        }
        Toast.makeText(this, "正在验证内核卡…", Toast.LENGTH_SHORT).show();
        new Thread(new Runnable() {
            public void run() {
                final String err = envLic.activate(MainActivity.this, cardTxt);
                runOnUiThread(new Runnable() {
                    public void run() {
                        if (err == null) {
                            Toast.makeText(MainActivity.this,
                                    "内核卡激活成功，剩余 " + envLic.daysLeft() + " 天", Toast.LENGTH_LONG).show();
                            etKernelCard.setText("");
                            refreshKernelLic();
                            refreshEnvStatus();
                        } else {
                            Toast.makeText(MainActivity.this, "激活失败：" + err, Toast.LENGTH_LONG).show();
                        }
                    }
                });
            }
        }).start();
    }

    void refreshKernelLic() {
        if (tvKernelLic == null) return;
        if (envLic.isValid()) {
            tvKernelLic.setTextColor(Color.parseColor("#2E7D32"));
            tvKernelLic.setText("内核卡：已激活，剩余 " + envLic.daysLeft() + " 天（绑定本机指纹）");
        } else {
            tvKernelLic.setTextColor(Color.parseColor("#FF6B6B"));
            tvKernelLic.setText("内核卡：未激活（普通卡密不能用于内核）");
        }
    }

    void refreshEnvStatus() {
        new Thread(new Runnable() {
            public void run() {
                final String inst = su("[ -f /data/adb/modules/hide_env_pack/root.id ] && echo 1 || echo 0").trim();
                final String runp = su("pgrep -f 'heguard|hide_env' >/dev/null 2>&1 && echo 1 || echo 0").trim();
                runOnUiThread(new Runnable() {
                    public void run() {
                        if (tvEnvStatus == null) return;
                        String a1 = "1".equals(inst) ? "已安装" : "未安装";
                        String a2 = "1".equals(runp) ? "运行中" : "未运行";
                        tvEnvStatus.setText("环境引擎状态：" + a1 + " · " + a2);
                        tvEnvStatus.setTextColor(Color.parseColor(
                                "1".equals(inst) ? "#2E7D32" : "#8A8A8E"));
                    }
                });
            }
        }).start();
    }

    /**
     * 打开环境软件（WGN TOOL）
     * 严格只做 Intent 拉起：不 source 它的脚本、不 pkill 它的进程、不改它的配置，
     * 环境引擎的一切启停/方案切换都只在它自己的界面里进行。
     */
    void envStart() {
        if (!envLic.isValid()) {
            Toast.makeText(this, "请先激活内核卡（第二层卡密）", Toast.LENGTH_LONG).show();
            return;
        }
        try {
            Intent i = getPackageManager().getLaunchIntentForPackage("com.wgntool.app");
            if (i == null) {
                Toast.makeText(this, "未检测到环境软件（com.wgntool.app）", Toast.LENGTH_LONG).show();
                return;
            }
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(i);
            Toast.makeText(this, "已打开环境软件", Toast.LENGTH_SHORT).show();
        } catch (Throwable t) {
            Toast.makeText(this, "打开失败：" + t.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }

    /** 只刷新状态显示，不碰环境引擎 */
    void envStop() {
        refreshEnvStatus();
        Toast.makeText(this, "已刷新状态（内核环境只能在本体里启停）", Toast.LENGTH_SHORT).show();
    }

    /** 退出界面自动落盘配置：换设备/重启都不用重新调 */
    @Override
    protected void onPause() {
        super.onPause();
        try { pushCfg(); } catch (Throwable t) { }
    }

    void pushCfg() {
        StringBuilder sb = new StringBuilder();
        sb.append("master=1\n");
        sb.append("dec=").append(dec[0] ? 1 : 0).append('\n');
        sb.append("dec2=").append(dec[1] ? 1 : 0).append('\n');
        sb.append("dec3=").append(dec[2] ? 1 : 0).append('\n');
        sb.append("dec4=").append(dec[3] ? 1 : 0).append('\n');
        for (int i = 0; i < 5; i++) sb.append("k").append(i).append("=").append(kind[i] ? 1 : 0).append('\n');
        sb.append("bone=").append(bone ? 1 : 0).append('\n');
        sb.append("lab=").append(lab ? 1 : 0).append('\n');
        sb.append("abar=").append(abar ? 1 : 0).append('\n');
        sb.append("fps=").append(fpsSel).append('\n');
        sb.append("price=").append(priceSel).append('\n');
        sb.append("rot=0\n");
        sb.append("wall=").append(0).append('\n');
        sb.append("radar=").append(radar ? 1 : 0).append('\n');
        sb.append("radrange=").append(RAD_RANGES[radRangeSel]).append('\n');
        sb.append("radrot=").append(radRot ? 1 : 0).append('\n');
        sb.append("radwho=").append(1).append(',')
          .append(radBots ? 1 : 0).append(',').append(radLoot ? 1 : 0).append('\n');
        sb.append("radpos=").append(RAD_POS[radPosSel][0]).append(',')
          .append(RAD_POS[radPosSel][1]).append(',').append(RAD_R).append('\n');
        try {
            File f = new File(local + "/ov_cfg.txt");
            FileOutputStream os = new FileOutputStream(f);
            os.write(sb.toString().getBytes("UTF-8"));
            os.close();
            su("grep '^aff=' " + DIR + "/ov_cfg.txt 2>/dev/null | head -1 >> " + local + "/ov_cfg.txt");
            su("cp -f " + local + "/ov_cfg.txt " + DIR + "/ov_cfg.txt && chmod 666 " + DIR + "/ov_cfg.txt");
        } catch (Throwable t) { }
    }

    // 创作者模式开关：写入 ov_creator，绘制端每帧热读
    void pushCreator() {
        prefs.edit().putBoolean("creator", creator).apply();
        write(local + "/ov_creator", creator ? "1\n" : "0\n");
        su("mkdir -p " + DIR + "; cp -f " + local + "/ov_creator " + DIR + "/ov_creator "
                + "&& chmod 666 " + DIR + "/ov_creator");
    }

    String su(String cmd) {
        try {
            Process pr = Runtime.getRuntime().exec(new String[] { "su", "-c", cmd });
            BufferedReader br = new BufferedReader(new InputStreamReader(pr.getInputStream()));
            StringBuilder sb = new StringBuilder();
            String l;
            while ((l = br.readLine()) != null) sb.append(l).append('\n');
            pr.waitFor();
            return sb.toString();
        } catch (Throwable t) {
            return "ERR " + t;
        }
    }

    void sleep(int ms) {
        try { Thread.sleep(ms); } catch (Throwable t) { }
    }
}
