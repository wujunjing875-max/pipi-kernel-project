package com.tpen.esp;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

/**
 * 卡密授权层 —— 对接 WGN 控制台（https://wjntool.top/wgn，即 47.82.71.105/wgn）
 *
 * 协议（实测）：
 *   · 激活  POST {base}/api/v1/activate  {"card","deviceId","name"}
 *           → {"ok":true,"authType":"card","license":{"token","expiresAt"(秒),...}}
 *           卡密错误 → ok:false + CARD_INVALID 等 → 不落盘 → 无法启动内核
 *   · 复核  POST {base}/api/v1/refresh   {"token","deviceId"}
 *   · 设备指纹 hwid：64 位 hex。优先复用环境模块 wgn_remote.conf 里的 device
 *     （本机在 WGN 里已登记的指纹）→ 环境 + 内核一 + 内核二（本 ESP）同机同指纹，
 *     卡绑一次，三个产品都能启动。
 *   · 服务器连不上 → 3 天宽限期，超时退回未激活。
 */
public class License {

    final MainActivity ctx;

    static final String DEF_URL = "https://wjntool.top/wgn";
    static final long GRACE_SEC = 3L * 86400L;

    // 设备指纹来源（按顺序找第一个 64hex）
    static final String ENV_WGN_CONF = "/data/adb/hide_env_pack/config/wgn_remote.conf";
    static final String OWN_DEVICE_FILE = "/data/adb/.wgn_device";

    // 授权状态落盘位置：内核二主/镜像 + 内核一互认（三产品同机通用）
    static final String[] SHARED = {
            "/data/local/tmp/.tpen/license/state",   // 内核二 主
            "/data/adb/.tpen_lic",                   // 内核二 镜像
            "/data/adb/k1/license/state",            // 内核一 主
            "/data/adb/.k1_lic",                     // 内核一 镜像
            "/data/local/tmp/.k1/license/state",     // 内核一 备用
            "/data/local/tmp/.k1_lic",
    };

    String hwid = "", server = "";
    Map<String, String> st = new HashMap<String, String>();
    String srcPath = "";

    License(MainActivity c) { ctx = c; }

    // ---------- 服务器地址 ----------
    String server() {
        if (server.length() > 0) return server;
        String u = ctx.su("cat /data/local/tmp/.tpen/wgn_url.txt 2>/dev/null | head -n 1 | tr -d ' \\r\\n'").trim();
        if (u.length() < 3) u = DEF_URL;
        if (u.startsWith("http://47.82.71.105/wgn")) u = DEF_URL;   // 统一走域名 + https
        while (u.endsWith("/")) u = u.substring(0, u.length() - 1);
        server = u;
        return server;
    }

    // ---------- 设备指纹（64hex，与环境/内核一同一台设备同一个值） ----------
    static String kv(String txt, String key) {
        if (txt == null) return "";
        String[] lines = txt.split("\n");
        for (String l : lines) {
            l = l.trim();
            if (l.startsWith(key + "=")) return l.substring(key.length() + 1).trim();
        }
        return "";
    }

    String hwid() {
        if (hwid.length() == 64) return hwid;
        // 1) 环境模块的 WGN 指纹（本机已登记 → 三产品天然同指纹）
        String d = kv(ctx.su("cat " + ENV_WGN_CONF + " 2>/dev/null"), "device");
        // 2) 自己落过的
        if (d.length() != 64) d = kv(ctx.su("cat " + OWN_DEVICE_FILE + " 2>/dev/null"), "device");
        // 3) 稳定派生：只用换真机才会变的硬件件
        if (d.length() != 64) {
            String cmd = "S=$(getprop ro.serialno); [ -n \"$S\" ] || S=$(getprop ro.boot.serialno); "
                    + "M=$(getprop ro.soc.model); [ -n \"$M\" ] || M=$(getprop ro.board.platform); "
                    + "H=$(getprop ro.boot.hardware); [ -n \"$H\" ] || H=$(getprop ro.hardware); "
                    + "B=$(getprop ro.product.cpu.abi); "
                    + "printf 'wgndev|%s|%s|%s|%s' \"$S\" \"$M\" \"$H\" \"$B\" | sha256sum | awk '{print $1}'";
            d = ctx.su(cmd).trim().replaceAll("[^0-9a-f]", "");
        }
        if (d.length() == 64) {
            hwid = d;
            ctx.su("mkdir -p /data/adb /data/local/tmp/.tpen; printf 'device=" + hwid + "\\n' > "
                    + OWN_DEVICE_FILE + " 2>/dev/null; chmod 600 " + OWN_DEVICE_FILE + " 2>/dev/null");
        }
        return hwid;
    }

    String member() {   // WGN 里没有会员号，显示套餐
        String p = st.get("plan");
        return (p == null || p.length() == 0) ? "-" : p;
    }

    // ---------- 扫描本机凭证：自己 / 内核一 / 环境模块，谁激活过就直接用 ----------
    boolean scan() {
        hwid();
        st = new HashMap<String, String>();
        srcPath = "";
        long best = -1;
        long now = System.currentTimeMillis() / 1000;
        // a) 内核二 / 内核一 的 state（格式：activated=1 + token + hwid）
        for (String p : SHARED) {
            Map<String, String> m = read(p);
            if (m.isEmpty() || !"1".equals(m.get("activated"))) continue;
            String h = m.get("hwid");
            if (h != null && h.length() > 0 && !h.equals(hwid)) continue;   // 别台设备的
            if (m.get("token") == null || m.get("token").length() < 10) continue;
            long exp = num(m.get("expire_at"));
            if (exp > 0 && now > exp) continue;
            long ok = num(m.get("last_ok_at"));
            if (ok > best) { best = ok; st = m; srcPath = p; }
        }
        // b) 环境模块的 WGN token（wgn_remote.conf：token= / device=）
        if (st.isEmpty()) {
            String conf = ctx.su("cat " + ENV_WGN_CONF + " 2>/dev/null");
            String tok = kv(conf, "token");
            String dev = kv(conf, "device");
            if (tok.length() > 10 && (dev.length() != 64 || dev.equals(hwid))) {
                st = new HashMap<String, String>();
                st.put("activated", "1");
                st.put("token", tok);
                st.put("hwid", dev.length() == 64 ? dev : hwid);
                st.put("plan", "环境模块（共用）");
                st.put("last_ok_at", "0");
                srcPath = ENV_WGN_CONF;
            }
        }
        return !st.isEmpty();
    }

    Map<String, String> read(String p) {
        Map<String, String> m = new HashMap<String, String>();
        String txt = ctx.su("cat " + p + " 2>/dev/null");
        if (txt == null || txt.length() < 3) return m;
        String[] lines = txt.split("\n");
        for (String l : lines) {
            l = l.trim();
            int i = l.indexOf('=');
            if (i <= 0) continue;
            m.put(l.substring(0, i), l.substring(i + 1).trim());
        }
        return m;
    }

    // ---------- 激活：卡密错误 → 返回错误、不写任何状态 → 内核无法启动 ----------
    String activate(String card) {
        card = card.trim().toUpperCase().replace(" ", "");
        if (card.length() == 0) return "卡密为空";
        if (hwid().length() != 64) return "设备指纹生成失败，请重试";
        String name = safe(gp("ro.product.brand")) + " " + safe(modelName());
        String body = "{\"card\":\"" + jesc(card) + "\",\"deviceId\":\"" + hwid + "\",\"name\":\"" + jesc(name) + "\"}";
        String r = post(server() + "/api/v1/activate", body);
        if (r == null) return "连不上服务器（" + server() + "）：检查网络后重试";
        if (!jok(r)) {
            String code = jget(r, "code");
            String msg = jget(r, "message");
            if (msg.length() == 0) msg = jget(r, "msg");
            return "服务器拒绝：" + (msg.length() > 0 ? msg : "未知原因")
                    + (code.length() > 0 ? "（" + code + "）" : "");
        }
        String token = jget(r, "token");
        if (token.length() < 10) return "服务器响应缺少授权令牌";
        long now = System.currentTimeMillis() / 1000;
        st = new HashMap<String, String>();
        st.put("activated", "1");
        st.put("card", card);
        st.put("token", token);
        st.put("hwid", hwid);
        String plan = jget(r, "planLabel");
        if (plan.length() == 0) plan = jget(r, "plan");
        st.put("plan", plan.length() > 0 ? plan : "已激活");
        st.put("expire_at", String.valueOf(num(jget(r, "expiresAt"))));
        st.put("last_ok_at", String.valueOf(now));
        st.put("last_msg", "激活成功");
        saveAll();
        return "OK";
    }

    // ---------- 复核（心跳）：服务器说卡被封/过期 → 自动退回未激活 ----------
    String heartbeat() {
        if (hwid().length() != 64) return "设备指纹异常";
        String tok = st.get("token");
        if (tok == null || tok.length() < 10) return "没有授权令牌，请重新激活";
        long now = System.currentTimeMillis() / 1000;
        long exp = num(st.get("expire_at"));
        if (exp > 0 && now > exp) {
            deactivate("卡密已到期");
            return "卡密已到期，请续费后重新激活";
        }
        String body = "{\"token\":\"" + jesc(tok) + "\",\"deviceId\":\"" + hwid + "\"}";
        String r = post(server() + "/api/v1/refresh", body);
        if (r == null) {
            long last = num(st.get("last_ok_at"));
            if (last > 0 && (now - last) <= GRACE_SEC) {
                st.put("last_msg", "服务器暂时连不上，宽限期内（3 天）");
                return "OK：离线宽限期内";
            }
            deactivate("长时间联系不上服务器（超过 3 天宽限）");
            return "长时间连不上服务器，已退回未激活";
        }
        if (!jok(r)) {
            String msg = jget(r, "message");
            if (msg.length() == 0) msg = jget(r, "msg");
            String code = jget(r, "code");
            deactivate("服务器已停用：" + (msg.length() > 0 ? msg : code));
            return "服务器未放行：" + (msg.length() > 0 ? msg : code);
        }
        // refresh 成功：可能换了新 token / 更新了到期时间
        String ntok = jget(r, "token");
        if (ntok.length() > 10) st.put("token", ntok);
        String ne = jget(r, "expiresAt");
        if (ne.length() > 0) st.put("expire_at", String.valueOf(num(ne)));
        st.put("grace_until", jget(r, "graceUntil"));
        st.put("last_ok_at", String.valueOf(now));
        st.put("last_msg", "复核通过");
        saveAll();
        return "OK";
    }

    void deactivate(String why) {
        st.put("activated", "0");
        st.put("last_msg", "已停用：" + why);
        saveAll();
    }

    boolean active() { return "1".equals(st.get("activated")); }

    // ---------- 落盘：写到全部共享位置，环境 / 内核一立即可认 ----------
    void saveAll() {
        StringBuilder sb = new StringBuilder();
        String[] keys = { "activated", "card", "token", "hwid", "plan",
                "expire_at", "grace_until", "last_ok_at", "last_msg" };
        for (String k : keys) {
            String v = st.get(k);
            if (v != null && v.length() > 0) sb.append(k).append('=').append(v).append('\n');
        }
        try {
            File f = new File(ctx.local + "/lic_state.tmp");
            FileOutputStream os = new FileOutputStream(f);
            os.write(sb.toString().getBytes("UTF-8"));
            os.close();
            String src = ctx.local + "/lic_state.tmp";
            StringBuilder cmd = new StringBuilder();
            cmd.append("mkdir -p /data/local/tmp/.tpen/license /data/adb/k1/license "
                    + "/data/local/tmp/.k1/license 2>/dev/null; ");
            for (String p : SHARED) {
                cmd.append("cp -f ").append(src).append(' ').append(p)
                        .append(" 2>/dev/null; chmod 600 ").append(p).append(" 2>/dev/null; ");
            }
            cmd.append("echo saved");
            ctx.su(cmd.toString());
        } catch (Throwable t) {
            android.util.Log.e("TPEN", "lic save fail " + t);
        }
    }

    // ---------- 设备信息 ----------
    String gp(String k) {
        String v = ctx.su("getprop " + k).trim();
        return v == null ? "" : v;
    }

    String modelName() {
        String m = gp("ro.product.marketname");
        if (m.length() == 0) m = gp("ro.product.vendor.marketname");
        if (m.length() == 0) m = gp("ro.product.model");
        if (m.length() == 0) m = gp("ro.product.device");
        return m;
    }

    String safe(String s) {
        if (s == null) return "";
        s = s.replace("\"", "").replace("\\", "").replace("\n", " ").trim();
        if (s.length() > 40) s = s.substring(0, 40);
        return s;
    }

    String jesc(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    // ---------- HTTP ----------
    String post(String url, String body) {
        try {
            HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
            c.setRequestMethod("POST");
            c.setRequestProperty("Content-Type", "application/json");
            c.setConnectTimeout(8000);
            c.setReadTimeout(12000);
            c.setDoOutput(true);
            OutputStream os = c.getOutputStream();
            os.write(body.getBytes("UTF-8"));
            os.close();
            int code = c.getResponseCode();
            InputStream is = (code >= 200 && code < 300) ? c.getInputStream() : c.getErrorStream();
            if (is == null) return null;
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = is.read(buf)) > 0) bo.write(buf, 0, n);
            is.close();
            return new String(bo.toByteArray(), "UTF-8");
        } catch (Throwable t) {
            android.util.Log.e("TPEN", "lic post fail " + t);
            return null;
        }
    }

    boolean jok(String json) {
        String s = json.replace(" ", "").replace("\n", "").replace("\r", "").replace("\t", "");
        return s.contains("\"ok\":true") || s.contains("\"ok\":1");
    }

    String jget(String json, String key) {
        String s = json.replace("\n", "").replace("\r", "");
        int i = s.indexOf("\"" + key + "\"");
        if (i < 0) return "";
        int j = s.indexOf(':', i);
        if (j < 0) return "";
        int k = j + 1;
        while (k < s.length() && (s.charAt(k) == ' ' || s.charAt(k) == '\t')) k++;
        if (k < s.length() && s.charAt(k) == '"') {
            int e = s.indexOf('"', k + 1);
            if (e < 0) return "";
            return s.substring(k + 1, e);
        }
        int e = k;
        while (e < s.length() && (Character.isDigit(s.charAt(e)) || s.charAt(e) == '-')) e++;
        return s.substring(k, e).trim();
    }

    long num(String s) {
        if (s == null) return 0;
        s = s.trim();
        if (s.length() == 0) return 0;
        try { return Long.parseLong(s); } catch (Throwable t) { return 0; }
    }

    String expireText() {
        String p = st.get("plan");
        if (p != null && p.contains("永久")) return "永久有效";
        long exp = num(st.get("expire_at"));
        if (exp <= 0) return "长期有效";
        java.text.SimpleDateFormat f = new java.text.SimpleDateFormat("yyyy-MM-dd HH:mm");
        return f.format(new java.util.Date(exp * 1000));
    }
}
