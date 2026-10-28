package com.jianshang.aim;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;

import java.io.File;
import java.io.IOException;

/** 目标来源接口：换成 UE4 内存读取时只替换实现即可 */
public interface AimProvider {
    /** 返回目标屏幕坐标 [x,y]（横屏 3392x2400 坐标系），无目标返回 null */
    float[] find(Context ctx, float cx, float cy, float radius);
}
