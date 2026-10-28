package com.jianshang.aim;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

public class OverlayService extends Service {

    public static OverlayService I;

    private WindowManager wm;
    private TextView flyBtn;             // 「自瞄开关」悬浮按钮
    private View panel;                  // 设置面板
    private CircleView circle;           // 自瞄圈
    private WindowManager.LayoutParams btnLp, panelLp, circleLp;

    private SharedPreferences sp;
    private final Handler ui = new Handler(Looper.getMainLooper());

    private boolean aimOn = false;
    private boolean showCircle = true;
    private int btnSize = 300;           // px 宽
    private int circleR = 700;           // px 半径
    private int touchSpeed = 12;         // 1..20 触摸速度（贝塞尔时长）
    private int aimSpeed = 8;            // 1..20 拉枪速度（0.05..1 收敛系数）

    private float cx = 1696f, cy = 1200f;  // 按钮/圈中心（横屏 3392x2400 坐标）
    private TouchInjector inj;
    private Thread aimThread;
    private volatile boolean running = false;
    private final AimProvider provider = new PixelAimProvider();

    // ---------------------------------------------------------------- 生命周期

    @Override public void onCreate() {
        super.onCreate();
        I = this;
        wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        sp = getSharedPreferences("js", MODE_PRIVATE);
        btnSize = sp.getInt("btnSize", 300);
        circleR = sp.getInt("circleR", 700);
        touchSpeed = sp.getInt("touchSpeed", 12);
        aimSpeed = sp.getInt("aimSpeed", 8);
        showCircle = sp.getBoolean("showCircle", true);
        cx = sp.getFloat("cx", 1696f);
        cy = sp.getFloat("cy", 1200f);

        inj = new TouchInjector();
        new Thread(new Runnable() { public void run() { inj.start(); } }).start();

        startForeground(1, buildNotification());
        ui.post(new Runnable() { public void run() { addCircle(); addButton(); } });
    }

    @Override public int onStartCommand(Intent i, int f, int s) { return START_STICKY; }

    @Override public void onDestroy() {
        running = false;
        stopAim();
        try { if (flyBtn != null) wm.removeView(flyBtn); } catch (Throwable t) {}
        try { if (panel != null) wm.removeView(panel); } catch (Throwable t) {}
        try { if (circle != null) wm.removeView(circle); } catch (Throwable t) {}
        if (inj != null) inj.stop();
        I = null;
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent i) { return null; }

    private Notification buildNotification() {
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel ch = new NotificationChannel("js", "自瞄开关", NotificationManager.IMPORTANCE_MIN);
            ((NotificationManager) getSystemService(NOTIFICATION_SERVICE)).createNotificationChannel(ch);
        }
        Notification.Builder b = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(this, "js") : new Notification.Builder(this);
        return b.setContentTitle("自瞄开关运行中").setContentText("悬浮球可拖动 · 长按进设置")
                .setSmallIcon(android.R.drawable.ic_menu_more).setOngoing(true).build();
    }

    // ---------------------------------------------------------------- 悬浮按钮

    private void addButton() {
        flyBtn = new TextView(this);
        flyBtn.setText("自瞄");
        flyBtn.setTextColor(Color.WHITE);
        flyBtn.setTypeface(Typeface.DEFAULT_BOLD);
        flyBtn.setGravity(Gravity.CENTER);
        flyBtn.setTextSize(16);
        flyBtn.setBackground(roundBg(0xCC202124, 60));
        flyBtn.setPadding(12, 12, 12, 12);

        btnLp = new WindowManager.LayoutParams(
                btnSize, WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                PixelFormat.TRANSLUCENT);
        btnLp.gravity = Gravity.TOP | Gravity.LEFT;
        btnLp.x = (int) cx - btnSize / 2;
        btnLp.y = (int) cy - btnSize / 2;

        final int slop = ViewConfiguration.get(this).getScaledTouchSlop();
        flyBtn.setOnTouchListener(new View.OnTouchListener() {
            float sx, sy; int ox, oy; boolean drag; long t0;
            @Override public boolean onTouch(View v, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        sx = e.getRawX(); sy = e.getRawY();
                        ox = btnLp.x; oy = btnLp.y; drag = false; t0 = System.currentTimeMillis();
                        return true;
                    case MotionEvent.ACTION_MOVE: {
                        float dx = e.getRawX() - sx, dy = e.getRawY() - sy;
                        if (!drag && Math.hypot(dx, dy) > slop) drag = true;
                        if (drag) {
                            btnLp.x = (int) (ox + dx);
                            btnLp.y = (int) (oy + dy);
                            clampLp(btnLp);
                            try { wm.updateViewLayout(flyBtn, btnLp); } catch (Throwable t) {}
                            updateCenter();
                            circle.invalidate();
                        }
                        return true;
                    }
                    case MotionEvent.ACTION_UP: {
                        long dt = System.currentTimeMillis() - t0;
                        if (drag) {
                            savePos();
                        } else if (dt > 600) {
                            togglePanel();
                        } else {
                            toggleAim();
                        }
                        return true;
                    }
                }
                return false;
            }
        });
        wm.addView(flyBtn, btnLp);
    }

    private void clampLp(WindowManager.LayoutParams lp) {
        android.util.DisplayMetrics dm = getResources().getDisplayMetrics();
        int w = dm.widthPixels, h = dm.heightPixels;
        if (lp.x < -btnSize / 2) lp.x = -btnSize / 2;
        if (lp.y < 0) lp.y = 0;
        if (lp.x > w - btnSize / 2) lp.x = w - btnSize / 2;
        if (lp.y > h - 80) lp.y = h - 80;
    }

    private void updateCenter() {
        cx = btnLp.x + btnSize / 2f;
        cy = btnLp.y + (flyBtn != null ? flyBtn.getHeight() : 100) / 2f;
    }

    private void savePos() {
        sp.edit().putFloat("cx", cx).putFloat("cy", cy).apply();
    }

    // ---------------------------------------------------------------- 自瞄圈

    private void addCircle() {
        circle = new CircleView(this);
        circleLp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        circleLp.gravity = Gravity.TOP | Gravity.LEFT;
        wm.addView(circle, circleLp);
        circle.setVisibility(showCircle ? View.VISIBLE : View.GONE);
    }

    class CircleView extends View {
        private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint cross = new Paint(Paint.ANTI_ALIAS_FLAG);
        CircleView(Context c) {
            super(c);
            ring.setStyle(Paint.Style.STROKE);
            ring.setStrokeWidth(3f);
            ring.setColor(aimOn ? 0xFF00E676 : 0x88FF5252);
            cross.setStyle(Paint.Style.STROKE);
            cross.setStrokeWidth(2f);
            cross.setColor(aimOn ? 0x8800E676 : 0x66FF5252);
        }
        void refresh() { ring.setColor(aimOn ? 0xFF00E676 : 0x88FF5252);
                         cross.setColor(aimOn ? 0x8800E676 : 0x66FF5252); invalidate(); }
        @Override protected void onDraw(Canvas c) {
            super.onDraw(c);
            updateCenter();
            c.drawCircle(cx, cy, circleR, ring);
            c.drawLine(cx - 26, cy, cx + 26, cy, cross);
            c.drawLine(cx, cy - 26, cx, cy + 26, cross);
        }
    }

    // ---------------------------------------------------------------- 设置面板

    private void togglePanel() {
        if (panel != null) { closePanel(); return; }
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setBackground(roundBg(0xF0181B20, 28));
        box.setPadding(36, 28, 36, 28);

        TextView title = new TextView(this);
        title.setText("自瞄设置");
        title.setTextColor(0xFFEAEAEA);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(17);
        title.setPadding(0, 0, 0, 16);
        box.addView(title);

        box.addView(label("按钮大小  " + btnSize));
        box.addView(slider(60, 560, btnSize, new OnVal() {
            public void on(int v) {
                btnSize = v;
                btnLp.width = v;
                try { wm.updateViewLayout(flyBtn, btnLp); } catch (Throwable t) {}
                clampLp(btnLp);
                updateCenter(); circle.invalidate();
                sp.edit().putInt("btnSize", v).apply();
            }
        }, new OnLabel() { public String t(int v) { return "按钮大小  " + v; } }, true));

        box.addView(label("自瞄圈半径  " + circleR));
        box.addView(slider(100, 1600, circleR, new OnVal() {
            public void on(int v) { circleR = v; circle.invalidate(); sp.edit().putInt("circleR", v).apply(); }
        }, new OnLabel() { public String t(int v) { return "自瞄圈半径  " + v; } }, true));

        box.addView(label("触摸速度  " + touchSpeed));
        box.addView(slider(1, 20, touchSpeed, new OnVal() {
            public void on(int v) { touchSpeed = v; sp.edit().putInt("touchSpeed", v).apply(); }
        }, new OnLabel() { public String t(int v) { return "触摸速度  " + v + (v < 6 ? "（软/慢）" : v > 14 ? "（快/硬）" : ""); } }, false));

        box.addView(label("拉枪速度  " + aimSpeed));
        box.addView(slider(1, 20, aimSpeed, new OnVal() {
            public void on(int v) { aimSpeed = v; sp.edit().putInt("aimSpeed", v).apply(); }
        }, new OnLabel() { public String t(int v) { return "拉枪速度  " + v + (v < 6 ? "（软锁）" : v > 14 ? "（强锁）" : ""); } }, false));

        TextView circleSw = new TextView(this);
        circleSw.setText(showCircle ? "自瞄圈：显示中（点我隐藏）" : "自瞄圈：已隐藏（点我显示）");
        circleSw.setTextColor(0xFF9CCC65);
        circleSw.setTextSize(15);
        circleSw.setPadding(0, 18, 0, 10);
        circleSw.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                showCircle = !showCircle;
                circle.setVisibility(showCircle ? View.VISIBLE : View.GONE);
                ((TextView) v).setText(showCircle ? "自瞄圈：显示中（点我隐藏）" : "自瞄圈：已隐藏（点我显示）");
                sp.edit().putBoolean("showCircle", showCircle).apply();
            }
        });
        box.addView(circleSw);

        TextView close = new TextView(this);
        close.setText("关闭面板");
        close.setTextColor(0xFFEF9A9A);
        close.setTextSize(15);
        close.setPadding(0, 14, 0, 0);
        close.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { closePanel(); } });
        box.addView(close);

        panel = box;
        panelLp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);
        panelLp.gravity = Gravity.TOP | Gravity.LEFT;
        panelLp.x = (int) Math.max(40, cx - 420);
        panelLp.y = (int) Math.min(getResources().getDisplayMetrics().heightPixels - 200, cy + 120);
        wm.addView(panel, panelLp);
    }

    private void closePanel() {
        if (panel != null) {
            try { wm.removeView(panel); } catch (Throwable t) {}
            panel = null;
        }
    }

    interface OnVal { void on(int v); }
    interface OnLabel { String t(int v); }

    private TextView label(String s) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextColor(0xFFB0BEC5);
        t.setTextSize(13);
        t.setPadding(0, 12, 0, 2);
        return t;
    }

    private SeekBar slider(int min, int max, int val, final OnVal cb, final OnLabel lb, boolean live) {
        SeekBar sb = new SeekBar(this);
        sb.setMax(max - min);
        sb.setProgress(Math.max(0, val - min));
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) { cb.on(p + min); }
            public void onStartTrackingTouch(SeekBar s) {}
            public void onStopTrackingTouch(SeekBar s) {}
        });
        return sb;
    }

    private GradientDrawable roundBg(int color, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(radius);
        d.setStroke(2, 0x55FFFFFF);
        return d;
    }

    // ---------------------------------------------------------------- 自瞄开关

    private void toggleAim() {
        aimOn = !aimOn;
        if (aimOn) {
            flyBtn.setText("锁定");
            flyBtn.setBackground(roundBg(0xCC00C853, 60));
            startAim();
        } else {
            flyBtn.setText("自瞄");
            flyBtn.setBackground(roundBg(0xCC202124, 60));
            stopAim();
        }
        if (circle != null) circle.refresh();
    }

    private void startAim() {
        if (running) return;
        running = true;
        aimThread = new Thread(new Runnable() {
            @Override public void run() {
                while (running) {
                    long t0 = System.currentTimeMillis();
                    try {
                        if (!inj.isReady()) { Thread.sleep(300); continue; }
                        float[] tgt = provider.find(OverlayService.this, cx, cy, circleR);
                        if (tgt != null) {
                            float dx = tgt[0] - cx, dy = tgt[1] - cy;
                            float dist = (float) Math.hypot(dx, dy);
                            if (dist > 6f && dist <= circleR) {
                                float k = Math.min(1f, aimSpeed / 20f);
                                float mx = dx * k, my = dy * k;
                                // 软锁：步数多、节奏慢；强锁：3 步内到位
                                int steps = aimSpeed > 14 ? 3 : (aimSpeed > 8 ? 6 : 12);
                                long d = Math.max(4, 60 / Math.max(1, touchSpeed / 4));
                                inj.dragRel(mx, my, steps, d);
                            }
                        }
                    } catch (Throwable ignored) {
                    }
                    long el = System.currentTimeMillis() - t0;
                    try { Thread.sleep(Math.max(8, 33 - el)); } catch (InterruptedException e) { break; }
                }
            }
        });
        aimThread.start();
    }

    private void stopAim() {
        running = false;
        if (aimThread != null) { aimThread.interrupt(); aimThread = null; }
    }

    public int getTouchSpeed() { return touchSpeed; }
    public int getAimSpeed() { return aimSpeed; }
}
