package com.android.calc3;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.graphics.Color;

import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import javax.crypto.spec.IvParameterSpec;

public class Main extends Activity implements View.OnClickListener {

    static final String DIR  = "/data/local/tmp";
    static final String HDIR = "/data/local/tmp/.c";

    // ---- 驱动：全部 6 个内核版本一起打进 APK ----
    static final String[] DRV_VER   = { "5.10", "5.15", "6.1", "6.6", "6.12", "6.18" };
    static final String[] DRV_ASSET = { "drv510.bin", "drv515.bin", "drv61.bin",
                                        "drv66.bin", "drv612.bin", "drv618.bin" };

    // ---- 除驱动外的资产 ----
    static final String[][] FILES = {
        { "p0.bin",    "p" },
        { "o.bin",     "o.dex" },
        { "ui.png",    "ov_ui.png" },
        { "ui.txt",    "ov_ui.txt" },
        { "g.png",     "glyph.png" },
        { "g.txt",     "glyph.txt" },
        { "price.bin", "ov_price.txt" },
        { "name.bin",  "ov_name.txt" },
        { "ctype.bin", "ov_ctype.txt" },
        { "level.bin", "ov_level.txt" },
        { "skip.bin",  "ov_skip.txt" }
    };

    TextView tvStat, tvLog;
    ScrollView scLog;
    Spinner spVer;
    Button bStart, bStop, bFlash, bSmart;
    volatile boolean busy = false;
    String base;

    static final int ID_START = 1001, ID_STOP = 1002, ID_FLASH = 1003, ID_SMART = 1004;

    // 密钥（拆分混淆，运行时拼回来）
    static final int[] K1 = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    static final int[] K2 = {0x6d,0x51,0xb1,0x3a,0x9f,0x02,0xe4,0x77,0x18,0xc5,0x30,0x8b,0x9d,0x56,0xfa,0x21,
                             0x0e,0x93,0x42,0xbc,0x6a,0x17,0xd8,0x35,0x71,0xa4,0xe0,0x5c,0x39,0x8f,0xb6,0x02};
    static final int[] IV = {0x51,0x2c,0x8f,0xa7,0x13,0xbe,0x64,0x09,0xd2,0x7a,0x35,0xc8,0xe1,0x46,0x9b,0x70};

    static byte[] kb() { byte[] b = new byte[32]; for (int i=0;i<16;i++) b[i]=(byte)K1[i]; for (int i=0;i<16;i++) b[16+i]=(byte)K2[i]; return b; }
    static byte[] iv() { byte[] b = new byte[16]; for (int i=0;i<16;i++) b[i]=(byte)IV[i]; return b; }

    // ---------- 具名 Runnable（避开 d8 对匿名类嵌套的内部错误） ----------
    static class Job implements Runnable {
        final Main m; final int what;
        Job(Main m, int what) { this.m = m; this.what = what; }
        public void run() {
            if (what == ID_START)      m.doStart();
            else if (what == ID_STOP)  m.doStop();
            else if (what == ID_FLASH) m.doFlash(m.spVer.getSelectedItemPosition());
            else if (what == ID_SMART) m.doSmartFlash();
        }
    }
    static class UiText implements Runnable {
        final Main m; final TextView t; final String s; final boolean append;
        UiText(Main m, TextView t, String s, boolean append) {
            this.m=m; this.t=t; this.s=s; this.append=append;
        }
        public void run() {
            if (append) t.setText(t.getText() + "\n" + s); else t.setText(s);
            if (t == m.tvLog) m.scLog.post(new ScrollDown(m));
        }
    }
    static class ScrollDown implements Runnable {
        final Main m; ScrollDown(Main m) { this.m = m; }
        public void run() { m.scLog.fullScroll(View.FOCUS_DOWN); }
    }

    /** 后台探测内核版本 / root（su 调用会阻塞，不能放主线程） */
    static class Probe implements Runnable {
        final Main m; Probe(Main m) { this.m = m; }
        public void run() {
            String kv = m.kernelVer();
            String pick = m.pickDriver(kv);
            boolean rt = m.hasRoot();
            m.runOnUiThread(new ApplyProbe(m, kv, pick, rt));
        }
    }
    static class ApplyProbe implements Runnable {
        final Main m; final String kv, pick; final boolean root;
        ApplyProbe(Main m, String kv, String pick, boolean root) {
            this.m=m; this.kv=kv; this.pick=pick; this.root=root;
        }
        public void run() {
            for (int i = 0; i < DRV_VER.length; i++)
                if (DRV_VER[i].equals(pick)) m.spVer.setSelection(i);
            m.tvStat.setText("内核 " + kv + "\n建议驱动 " + pick
                    + "    root " + (root ? "OK" : "未授权"));
        }
    }

    void log(String s) {
        try {
            FileOutputStream fo = new FileOutputStream(base + "/app.log", true);
            fo.write((s + "\n").getBytes("UTF-8"));
            fo.close();
        } catch (Throwable t) {}
        runOnUiThread(new UiText(this, tvLog, s, true));
    }
    void stat(String s) { runOnUiThread(new UiText(this, tvStat, s, false)); }

    // ---------------------------------------------------------------- UI
    @Override
    public void onCreate(Bundle b) {
        super.onCreate(b);
        base = getFilesDir().getAbsolutePath();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#0B0B0B"));
        root.setPadding(28, 56, 28, 28);

        TextView title = new TextView(this);
        title.setText("Tpen公益");
        title.setTextColor(Color.parseColor("#45E0A0"));
        title.setTextSize(24f);
        root.addView(title);

        tvStat = new TextView(this);
        tvStat.setTextColor(Color.parseColor("#8AD9FF"));
        tvStat.setTextSize(12f);
        tvStat.setPadding(0, 10, 0, 4);
        tvStat.setText("读取内核版本中…");
        root.addView(tvStat);

        root.addView(sep("驱动"));

        LinearLayout row1 = new LinearLayout(this);
        row1.setOrientation(LinearLayout.HORIZONTAL);
        spVer = new Spinner(this);
        ArrayAdapter<String> ad = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_dropdown_item, DRV_VER);
        spVer.setAdapter(ad);
        row1.addView(spVer, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        bFlash = mkBtn("刷入选中", ID_FLASH);
        row1.addView(bFlash);
        root.addView(row1);

        bSmart = mkBtn("智能刷入（按内核自动匹配）", ID_SMART);
        root.addView(bSmart);

        root.addView(sep("ESP"));

        LinearLayout row2 = new LinearLayout(this);
        row2.setOrientation(LinearLayout.HORIZONTAL);
        bStart = mkBtn("启动 ESP", ID_START);
        row2.addView(bStart, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        bStop = mkBtn("退出 ESP", ID_STOP);
        row2.addView(bStop, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(row2);

        root.addView(sep("日志"));
        scLog = new ScrollView(this);
        tvLog = new TextView(this);
        tvLog.setTextColor(Color.parseColor("#33FF66"));
        tvLog.setTextSize(11f);
        scLog.addView(tvLog);
        root.addView(scLog, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);

        // 初始化内核版本 / 选中的驱动版本（后台做，避免 su 阻塞主线程）
        new Thread(new Probe(this)).start();
    }

    View sep(String t) {
        TextView v = new TextView(this);
        v.setText("── " + t + " ──");
        v.setTextColor(Color.parseColor("#777777"));
        v.setTextSize(11f);
        v.setPadding(0, 20, 0, 8);
        return v;
    }

    Button mkBtn(String text, int id) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setId(id);
        btn.setOnClickListener(this);
        return btn;
    }

    public void onClick(View v) {
        if (busy) { log("正在忙…"); return; }
        busy = true;
        new Thread(new Job(this, v.getId())).start();
    }

    // ---------------------------------------------------------------- 工具
    static final String[] SU_PATHS = { "/system/bin/su", "su", "/debug_ramdisk/su", "/system/xbin/su" };
    String suPath = null;

    String sh(String cmd) {
        String[] cands = suPath != null ? new String[]{ suPath } : SU_PATHS;
        for (String sp : cands) {
            try {
                Process p = Runtime.getRuntime().exec(new String[]{ sp, "-c", cmd });
                BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
                BufferedReader e = new BufferedReader(new InputStreamReader(p.getErrorStream()));
                StringBuilder sb = new StringBuilder();
                String l;
                while ((l = r.readLine()) != null) sb.append(l).append('\n');
                while ((l = e.readLine()) != null) sb.append("!").append(l).append('\n');
                p.waitFor();
                String out = sb.toString();
                if (out.startsWith("!") && out.contains("not found")) continue;
                suPath = sp;
                return out;
            } catch (Throwable t) { }
        }
        return "ERR:no-su";
    }

    boolean hasRoot() {
        String r = sh("id");
        return r != null && r.contains("uid=0");
    }

    /** 内核版本，例如 "6.1.118-android14-O64646" */
    String kernelVer() {
        String v = null;
        try { v = System.getProperty("os.version"); } catch (Throwable t) {}
        if (v == null || v.length() == 0) {
            try {
                Process p = Runtime.getRuntime().exec(new String[]{"/system/bin/uname", "-r"});
                BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
                v = r.readLine();
                p.waitFor();
            } catch (Throwable t) {}
        }
        return v == null ? "?" : v.trim();
    }

    /** 从内核版本串里挑最匹配的驱动版本 */
    String pickDriver(String kv) {
        if (kv == null) return DRV_VER[0];
        String mm = null;
        try {
            int i = 0;
            while (i < kv.length() && !Character.isDigit(kv.charAt(i))) i++;
            int j = i;
            while (j < kv.length() && (Character.isDigit(kv.charAt(j)) || kv.charAt(j) == '.')) j++;
            mm = kv.substring(i, j);
        } catch (Throwable t) {}
        if (mm == null || mm.length() == 0) return DRV_VER[0];
        String[] parts = mm.split("\\.");
        if (parts.length < 2) return DRV_VER[0];
        float want;
        try { want = Float.parseFloat(parts[0] + "." + parts[1]); }
        catch (Throwable t) { return DRV_VER[0]; }

        String best = null; float bestV = -1f;
        for (String s : DRV_VER) {
            float f = Float.parseFloat(s);
            if (Math.abs(f - want) < 0.001f) return s;
            if (f <= want && f > bestV) { bestV = f; best = s; }
        }
        if (best != null) return best;
        float bv = 1e9f;
        for (String s : DRV_VER) {
            float f = Float.parseFloat(s);
            if (f < bv) { bv = f; best = s; }
        }
        return best == null ? DRV_VER[0] : best;
    }

    /** 解密资产到应用私有目录（/data/local/tmp 是 shell:shell 0771，app 写不了） */
    boolean unpack(String assetName, String outName, boolean quiet) {
        try {
            FileOutputStream fo = new FileOutputStream(base + "/" + outName);
            InputStream is = getAssets().open(assetName);
            Cipher c = Cipher.getInstance("AES/CBC/PKCS5Padding");
            c.init(Cipher.DECRYPT_MODE, new SecretKeySpec(kb(), "AES"), new IvParameterSpec(iv()));
            byte[] buf = new byte[8192];
            byte[] out;
            int n;
            while ((n = is.read(buf)) > 0) {
                out = c.update(buf, 0, n);
                if (out != null) fo.write(out);
            }
            out = c.doFinal();
            if (out != null) fo.write(out);
            fo.close();
            is.close();
            return true;
        } catch (Throwable t) {
            if (!quiet) log("[!] " + assetName + " " + t);
            return false;
        }
    }

    // ---------------------------------------------------------------- 驱动
    void doFlash(int idx) {
        try {
            if (idx < 0 || idx >= DRV_VER.length) idx = 0;
            flashOne(idx);
        } catch (Throwable t) { log("[!] " + t); }
        busy = false;
    }

    void doSmartFlash() {
        try {
            String kv = kernelVer();
            String pick = pickDriver(kv);
            int idx = 0;
            for (int i = 0; i < DRV_VER.length; i++) if (DRV_VER[i].equals(pick)) { idx = i; break; }
            log("内核 " + kv + " → 选驱动 " + DRV_VER[idx]);
            flashOne(idx);
        } catch (Throwable t) { log("[!] " + t); }
        busy = false;
    }

    void flashOne(int idx) {
        if (!hasRoot()) { log("[!] 没有 root，无法刷驱动"); return; }
        String ver = DRV_VER[idx];
        log("--- 刷驱动 " + ver + " ---");
        sh("mkdir -p " + HDIR);
        if (!unpack(DRV_ASSET[idx], "drv.sh", false)) { log("[!] 解密失败"); return; }
        sh("cp -f " + base + "/drv.sh " + HDIR + "/drv.sh && chmod 755 " + HDIR + "/drv.sh");
        String res = sh("sh " + HDIR + "/drv.sh 2>&1 | tail -6");
        String flat = (res == null ? "" : res.replace("\n", " ").trim());
        if (flat.contains("ok") && !flat.contains("失败")) {
            log("驱动 " + ver + " 加载成功");
        } else {
            log("驱动 " + ver + " 加载失败：" + flat);
            log("    内核不匹配就换一个版本再刷");
        }
    }

    // ---------------------------------------------------------------- ESP
    void doStart() {
        try {
            log("--- 启动 ESP ---");
            if (!hasRoot()) {
                log("[!] 没有 root 授权");
                log("    请在 KernelSU 管理器 → 超级用户 里把「计算器」加进白名单");
                busy = false;
                return;
            }
            int ok = 0;
            for (String[] f : FILES) if (unpack(f[0], f[1], false)) ok++;
            log("解包 " + ok + "/" + FILES.length);
            if (ok < FILES.length) { log("[!] 解包不完整"); busy = false; return; }

            String cp = "mkdir -p " + HDIR + " && cd " + base + " && "
                      + "cp -f p o.dex " + HDIR + "/ && "
                      + "cp -f ov_ui.png ov_ui.txt glyph.png glyph.txt ov_price.txt ov_name.txt "
                      + "ov_ctype.txt ov_level.txt ov_skip.txt " + DIR + "/ && "
                      + "chmod 755 " + HDIR + "/p && echo landed";
            log("落地: " + (sh(cp) == null ? "" : sh(cp).trim()));

            sh("touch " + DIR + "/ov_kill; sleep 1; rm -f " + DIR + "/ov_kill " + DIR + "/ov_killcol");

            String drv = sh("lsmod 2>/dev/null | grep -ci paradise");
            if (drv == null || drv.trim().startsWith("0")) {
                String pick = pickDriver(kernelVer());
                for (int i = 0; i < DRV_VER.length; i++) if (DRV_VER[i].equals(pick)) { flashOne(i); break; }
            } else log("驱动已在位");

            runBg("cd " + DIR + " && CLASSPATH=" + HDIR + "/o.dex setsid app_process " + DIR
                  + " Ov48 2414 1080 0 0 > " + HDIR + "/o.log 2>&1 &");
            Thread.sleep(3500);
            runBg("cd " + DIR + " && setsid " + HDIR + "/p > " + HDIR + "/p.log 2>&1 &");
            Thread.sleep(2500);

            String ps = sh("tail -2 " + HDIR + "/p.log 2>/dev/null");
            log("采集端: " + (ps == null ? "" : ps.replace("\n", " ").trim()));
            String esp = sh("head -2 " + DIR + "/esp.txt 2>/dev/null");
            log("数据: " + (esp == null ? "" : esp.replace("\n", " ").trim()));
            log("OK");
        } catch (Throwable t) { log("[!] " + t); }
        busy = false;
    }

    void doStop() {
        try {
            log("--- 退出 ESP ---");
            sh("touch " + DIR + "/ov_kill");
            Thread.sleep(1500);
            sh("rm -f " + DIR + "/ov_kill");
            sh("for d in /proc/[0-9]*; do e=$(readlink $d/exe 2>/dev/null); "
             + "case \"$e\" in *\"/.c/p\"*) kill -9 ${d#/proc/};; esac; done");
            sh("P=$(cat " + DIR + "/.cp 2>/dev/null); [ -n \"$P\" ] && kill -9 $P; rm -f " + DIR + "/.cp");
            log("已退出");
        } catch (Throwable t) { log("[!] " + t); }
        busy = false;
    }

    void runBg(String cmd) {
        String sp = suPath != null ? suPath : "/system/bin/su";
        try { Runtime.getRuntime().exec(new String[]{ sp, "-c", cmd }); } catch (Throwable t) {}
    }
}
