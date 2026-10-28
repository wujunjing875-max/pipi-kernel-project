package com.jianshang.aim;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;

import java.io.File;
import java.io.IOException;

/**
 * 视觉目标源（第一版）：root 截屏 -> 找自瞄圈内"红色能量重心"（血条/人物高亮）
 * 之后上 UE4 内存（UWorld -> Actors -> Head 屏幕投影）时，直接换掉这个类即可。
 */
public class PixelAimProvider implements AimProvider {

    private static final String SHOT = "/data/local/tmp/js_aim.png";
    private long lastShot = 0;

    @Override
    public float[] find(Context ctx, float cx, float cy, float radius) {
        long now = System.currentTimeMillis();
        if (now - lastShot < 45) return null;   // 限速，避免截图进程堆积
        lastShot = now;
        try {
            shoot();
            Bitmap full = BitmapFactory.decodeFile(SHOT);
            if (full == null) return null;
            int sw = full.getWidth(), sh = full.getHeight();
            // 降采样：宽不超过 360
            int dw = 360, dh = Math.max(1, sh * dw / sw);
            Bitmap small = Bitmap.createScaledBitmap(full, dw, dh, false);
            full.recycle();

            float sx = (float) sw / dw, sy = (float) sh / dh;
            float bestScore = 0; float bx = 0, by = 0; int cnt = 0;
            long sumX = 0, sumY = 0;

            int r = (int) radius;
            for (int y = 0; y < dh; y++) {
                for (int x = 0; x < dw; x++) {
                    float px = x * sx, py = y * sy;
                    if (Math.hypot(px - cx, py - cy) > r) continue;
                    int c = small.getPixel(x, y);
                    int R = Color.red(c), G = Color.green(c), B = Color.blue(c);
                    int score = R - Math.max(G, B);
                    if (R > 120 && score > 55) {
                        sumX += (long) px; sumY += (long) py; cnt++;
                        if (score > bestScore) { bestScore = score; bx = px; by = py; }
                    }
                }
            }
            small.recycle();
            if (cnt < 12) return null;           // 噪点过滤
            // 重心 + 最强点混合，避免整片红墙被当目标
            float gx = (float) sumX / cnt, gy = (float) sumY / cnt;
            return new float[]{ (gx * 2 + bx) / 3f, (gy * 2 + by) / 3f };
        } catch (Throwable t) {
            return null;
        }
    }

    private void shoot() throws IOException, InterruptedException {
        Process p = Runtime.getRuntime().exec(new String[]{
                "su", "-c", "screencap -p " + SHOT + " && chmod 666 " + SHOT});
        p.waitFor();
    }
}
