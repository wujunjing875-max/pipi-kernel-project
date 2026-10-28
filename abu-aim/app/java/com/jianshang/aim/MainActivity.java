package com.jianshang.aim;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.InputStreamReader;

public class MainActivity extends Activity {

    private TextView status;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);

        ScrollView sv = new ScrollView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(48, 64, 48, 64);
        root.setBackgroundColor(0xFF121418);
        sv.addView(root);

        TextView title = new TextView(this);
        title.setText("自瞄开关");
        title.setTextColor(Color.WHITE);
        title.setTextSize(26);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        TextView sub = new TextView(this);
        sub.setText("悬浮按钮 · 位置可拖 · 大小可调 · 自瞄圈可隐");
        sub.setTextColor(0xFF9AA6B2);
        sub.setTextSize(13);
        sub.setGravity(Gravity.CENTER);
        sub.setPadding(0, 8, 0, 36);
        root.addView(sub);

        status = new TextView(this);
        status.setTextColor(0xFFB0BEC5);
        status.setTextSize(13);
        status.setPadding(0, 0, 0, 28);
        root.addView(status);

        root.addView(btn("① 授予悬浮窗权限", 0xFF37474F, new View.OnClickListener() {
            public void onClick(View v) {
                Intent i = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(i);
            }
        }));

        root.addView(btn("② 启动悬浮按钮", 0xFF00897B, new View.OnClickListener() {
            public void onClick(View v) {
                if (Build.VERSION.SDK_INT >= 26) startForegroundService(new Intent(MainActivity.this, OverlayService.class));
                else startService(new Intent(MainActivity.this, OverlayService.class));
                toastStatus("已启动，桌面/游戏里看悬浮球");
            }
        }));

        root.addView(btn("③ 停止悬浮服务", 0xFF5D4037, new View.OnClickListener() {
            public void onClick(View v) {
                stopService(new Intent(MainActivity.this, OverlayService.class));
                toastStatus("已停止");
            }
        }));

        root.addView(btn("④ 检测 root", 0xFF455A64, new View.OnClickListener() {
            public void onClick(View v) { checkRoot(); }
        }));

        TextView help = new TextView(this);
        help.setText("\n操作说明\n"
                + "· 单击悬浮球 = 开/关自瞄\n"
                + "· 拖动悬浮球 = 调位置（自瞄圈跟随）\n"
                + "· 长按悬浮球 = 打开设置面板\n"
                + "· 面板内可调：按钮大小、圈半径、触摸速度、拉枪速度、圈显隐\n"
                + "· 触摸速度小=软锁慢滑，大=硬锁快拉\n"
                + "· 首次使用 KernelSU 会弹 root 授权，务必点允许\n");
        help.setTextColor(0xFF8C9AA8);
        help.setTextSize(13);
        help.setLineSpacing(8, 1f);
        root.addView(help);

        setContentView(sv);

        checkRoot();
    }

    private Button btn(String text, int color, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setTextSize(16);
        b.setBackgroundColor(color);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 150);
        lp.bottomMargin = 20;
        b.setLayoutParams(lp);
        b.setOnClickListener(l);
        return b;
    }

    private void toastStatus(String s) {
        status.setText(s + "\n悬浮窗权限：" + (Settings.canDrawOverlays(this) ? "已授予" : "未授予"));
    }

    private void checkRoot() {
        new Thread(new Runnable() {
            public void run() {
                final String r = execRoot();
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    public void run() { toastStatus("root：" + r); }
                });
            }
        }).start();
    }

    private String execRoot() {
        try {
            Process p = Runtime.getRuntime().exec(new String[]{"su", "-c", "id"});
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line = br.readLine();
            p.waitFor();
            return line == null ? "失败(无输出)" : line.trim();
        } catch (Throwable t) {
            return "失败: " + t.getMessage();
        }
    }
}
