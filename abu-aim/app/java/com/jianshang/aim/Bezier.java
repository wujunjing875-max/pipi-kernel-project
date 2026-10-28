package com.jianshang.aim;

/**
 * 贝塞尔轨迹生成（对应易语言源码的思路：三次贝塞尔 + easeOut 缓动 + 法线随机偏移）
 * 软锁：全程贝塞尔；强锁：steps 少、jitter 小，接近直线瞬移
 */
public class Bezier {

    /** 缓动：起步快、末端减速，最接近人手 */
    public static float easeOut(float t) {
        float u = 1f - t;
        return 1f - u * u * u;
    }

    /** easeInOut：两头慢中间快 */
    public static float easeInOut(float t) {
        return t < 0.5f ? 4f * t * t * t : 1f - (float) Math.pow(-2f * t + 2f, 3) / 2f;
    }

    /**
     * 返回 steps+1 个 [x,y] 点
     * 控制点沿法线随机偏移，形成自然弧度
     */
    public static float[][] path(float x0, float y0, float x1, float y1, int steps, float jitter) {
        steps = Math.max(2, steps);
        float dx = x1 - x0, dy = y1 - y0;
        float len = (float) Math.hypot(dx, dy);
        if (len < 1f) len = 1f;

        // 单位法线
        float nx = -dy / len, ny = dx / len;

        // 控制点：距起点/终点约 1/3 路径长，法线方向随机偏移
        float c1t = 0.28f + (float) Math.random() * 0.12f;
        float c2t = 0.60f + (float) Math.random() * 0.12f;
        float o1 = (float) ((Math.random() * 2 - 1) * jitter);
        float o2 = (float) ((Math.random() * 2 - 1) * jitter);

        float p1x = x0 + dx * c1t + nx * o1;
        float p1y = y0 + dy * c1t + ny * o1;
        float p2x = x0 + dx * c2t + nx * o2;
        float p2y = y0 + dy * c2t + ny * o2;

        float[][] out = new float[steps + 1][2];
        for (int i = 0; i <= steps; i++) {
            float t = easeInOut((float) i / steps);
            float u = 1f - t;
            // 三次贝塞尔 B(t)= (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
            float b0 = u * u * u;
            float b1 = 3f * u * u * t;
            float b2 = 3f * u * t * t;
            float b3 = t * t * t;
            out[i][0] = b0 * x0 + b1 * p1x + b2 * p2x + b3 * x1;
            out[i][1] = b0 * y0 + b1 * p1y + b2 * p2y + b3 * y1;
        }
        return out;
    }
}
