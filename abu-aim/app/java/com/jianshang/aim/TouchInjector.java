package com.jianshang.aim;

import java.io.IOException;
import java.io.OutputStream;

/**
 * 触摸注入器 —— 走 /dev/input/event4（真实触摸屏节点）
 * root shell 常驻：exec 3>/dev/input/event4，之后每条 printf 单事件写入（无 fork，微秒级）
 */
public class TouchInjector {

    private static final String DEV = "/dev/input/event4";
    private static final String Z16 =
        "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00";

    private Process su;
    private OutputStream out;
    private int trackingId = 5000;
    private boolean ready = false;
    private boolean down = false;

    // 当前指针位置（屏幕坐标，横屏 3392x2400 坐标系）
    public volatile float curX = 1696f;
    public volatile float curY = 1200f;

    public boolean isReady() { return ready; }

    public synchronized boolean start() {
        try {
            su = Runtime.getRuntime().exec(new String[]{"su"});
            out = su.getOutputStream();
            cmd("Z='" + Z16 + "'");
            cmd("exec 3>" + DEV);
            cmd("echo JS_READY");
            ready = true;
        } catch (Throwable t) {
            ready = false;
        }
        return ready;
    }

    public synchronized void stop() {
        try {
            if (out != null) { out.write("exec 3>&-\nexit\n".getBytes()); out.flush(); }
            if (su != null) su.destroy();
        } catch (Throwable ignored) {}
        ready = false;
        down = false;
    }

    private void cmd(String s) {
        try {
            if (out == null) return;
            out.write((s + "\n").getBytes());
            out.flush();
        } catch (IOException ignored) {}
    }

    private static String h2(int v) {
        return String.format("\\x%02x\\x%02x", v & 0xFF, (v >> 8) & 0xFF);
    }

    private static String h4(int v) {
        return String.format("\\x%02x\\x%02x\\x%02x\\x%02x",
                v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
    }

    /** 单条内核事件：type/code/value，24 字节 input_event（aarch64，timeval 置 0 由内核补） */
    private void ev(int type, int code, int value) {
        cmd("printf \"$Z" + h2(type) + h2(code) + h4(value) + "\" >&3");
    }

    /** 屏幕坐标(横屏 3392x2400) -> 触摸面板坐标(touchX=屏幕Y*10, touchY=屏幕X*10) */
    private static int tpX(float screenY) { return Math.round(screenY * 10f); }
    private static int tpY(float screenX) { return Math.round(screenX * 10f); }

    public synchronized void touchDown(float x, float y) {
        if (!ready) return;
        curX = x; curY = y;
        trackingId = (trackingId + 1) & 0x7FFF;
        ev(3, 47, 0);                    // ABS_MT_SLOT
        ev(3, 57, trackingId);           // ABS_MT_TRACKING_ID
        ev(3, 53, tpX(y));               // ABS_MT_POSITION_X
        ev(3, 54, tpY(x));               // ABS_MT_POSITION_Y
        ev(1, 330, 1);                   // BTN_TOUCH down
        ev(0, 0, 0);                     // SYN_REPORT
        down = true;
    }

    /** 抬起前若已 down，则收尾 */
    public synchronized void touchUp() {
        if (!ready || !down) return;
        ev(3, 57, -1);
        ev(1, 330, 0);
        ev(0, 0, 0);
        down = false;
    }

    /** 移动到绝对坐标（需已 down） */
    public synchronized void touchMove(float x, float y) {
        if (!ready || !down) return;
        curX = x; curY = y;
        ev(3, 53, tpX(y));
        ev(3, 54, tpY(x));
        ev(0, 0, 0);
    }

    public synchronized void tap(float x, float y) {
        touchDown(x, y);
        touchUp();
    }

    /** 相对拖动（自瞄拉枪用）：从当前位置拖到 cur+dx,cur+dy，steps 步 */
    public synchronized void dragRel(float dx, float dy, int steps, long frameDelayMs) {
        if (!ready) return;
        boolean startedHere = !down;
        if (startedHere) touchDown(curX, curY);
        float x0 = curX, y0 = curY, x1 = x0 + dx, y1 = y0 + dy;
        for (int i = 1; i <= steps; i++) {
            float t = (float) i / steps;
            float e = Bezier.easeOut(t);
            touchMove(x0 + (x1 - x0) * e, y0 + (y1 - y0) * e);
            if (frameDelayMs > 0) {
                try { Thread.sleep(frameDelayMs); } catch (InterruptedException ignored) {}
            }
        }
        if (startedHere) touchUp();
    }

    /**
     * 贝塞尔拟人滑动：起点 -> 终点，durationMs 总时长，steps 事件数，jitter 法线抖动(px)
     * 用于软锁：轨迹带弧度 + 缓动，避免直线均速被判定为机器
     */
    public synchronized void swipeBezier(float x0, float y0, float x1, float y1,
                                         int steps, long durationMs, float jitter) {
        if (!ready) return;
        float[][] pts = Bezier.path(x0, y0, x1, y1, steps, jitter);
        long delay = Math.max(0, durationMs / Math.max(1, steps));
        touchDown(pts[0][0], pts[0][1]);
        for (int i = 1; i < pts.length; i++) {
            touchMove(pts[i][0], pts[i][1]);
            if (delay > 0) {
                try { Thread.sleep(delay); } catch (InterruptedException ignored) {}
            }
        }
        touchUp();
    }
}
