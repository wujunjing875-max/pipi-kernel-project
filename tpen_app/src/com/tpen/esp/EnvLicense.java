package com.tpen.esp;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;

/**
 * 第二层卡密：内核卡（MF-XXXX-XXXX-XXXX-XXXX）
 * <p>
 * 第一层是普通卡密（License.java，控制绘制/ESP）；
 * 第二层是内核卡，单独控制「内核环境引擎」（hide_env_pack / hguard）。
 * <p>
 * 云端接口（与现有 /wgn/api/v1 同一套风格）：
 * POST /wgn/api/v1/kernel/activate   {"card","deviceId","name"}
 * → {"ok":true,"data":{"token":"...","expiresAt":秒}}
 * POST /wgn/api/v1/kernel/refresh    {"token","deviceId"}   （心跳，服务端待补）
 */
public class EnvLicense {

    public static final String BASE = "https://wjntool.top/wgn";
    public static final String PREF = "tpen_kernel";

    public String token = "";
    public long expiresAt = 0;      // 秒
    public String lastErr = "";
    private final SharedPreferences sp;

    public EnvLicense(Context c) {
        sp = c.getSharedPreferences(PREF, Context.MODE_PRIVATE);
        token = sp.getString("token", "");
        expiresAt = sp.getLong("expiresAt", 0);
    }

    /** 是否已激活且未过期（离线宽限 3 天，与普通卡密一致） */
    public boolean isValid() {
        if (token == null || token.length() < 8) return false;
        long now = System.currentTimeMillis() / 1000;
        return now < expiresAt + 3 * 86400L;
    }

    public long daysLeft() {
        long now = System.currentTimeMillis() / 1000;
        long d = (expiresAt - now) / 86400L;
        return d < 0 ? 0 : d;
    }

    /** 设备指纹：优先读环境引擎写下的 device=，没有就用本地生成并落盘 */
    public String deviceId(Context c) {
        String dev = readConfDevice();
        if (dev != null && dev.length() >= 32) return dev;
        String cached = sp.getString("dev", "");
        if (cached.length() >= 32) return cached;
        String raw = "tpen|" + android.os.Build.SERIAL + "|" + android.os.Build.SOC_MANUFACTURER
                + "|" + android.os.Build.HARDWARE + "|" + android.os.Build.MODEL;
        String h = sha256(raw);
        sp.edit().putString("dev", h).apply();
        return h;
    }

    private String readConfDevice() {
        String[] paths = {
                "/data/adb/hide_env_pack/config/wgn_remote.conf",
                "/data/adb/hide_env_pack/config/wgn.conf"
        };
        for (String p : paths) {
            try {
                File f = new File(p);
                if (!f.exists()) continue;
                BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(f), "UTF-8"));
                String ln;
                while ((ln = br.readLine()) != null) {
                    ln = ln.trim();
                    if (ln.startsWith("device=")) {
                        br.close();
                        return ln.substring(7).trim();
                    }
                }
                br.close();
            } catch (Throwable ignored) {
            }
        }
        return null;
    }

    public static String sha256(String s) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] d = md.digest(s.getBytes("UTF-8"));
            StringBuilder sb = new StringBuilder();
            for (byte b : d) sb.append(String.format("%02x", b));
            return sb.toString();
        } catch (Throwable t) {
            return "0";
        }
    }

    /** 激活内核卡。返回 null=成功，否则返回错误文案 */
    public String activate(Context c, String card) {
        lastErr = "";
        card = card == null ? "" : card.trim().toUpperCase();
        if (!card.matches("MF-[A-Z0-9]{4,6}-[A-Z0-9]{4,6}-[A-Z0-9]{4,6}-[A-Z0-9]{4,6}")) {
            return "格式不对，应为 MF-XXXX-XXXX-XXXX-XXXX";
        }
        String dev = deviceId(c);
        String body = "{\"card\":\"" + card + "\",\"deviceId\":\"" + dev + "\",\"name\":\"tpen-kernel\"}";
        String resp = post(BASE + "/api/v1/kernel/activate", body);
        if (resp == null) return "网络不通，连不上服务器";
        if (resp.contains("\"ok\":true")) {
            String tk = pick(resp, "token");
            String ex = pick(resp, "expiresAt");
            long exp = 0;
            try {
                exp = Long.parseLong(ex.replaceAll("[^0-9]", ""));
            } catch (Throwable ignored) {
            }
            if (exp > 100000000000L) exp = exp / 1000;   // 毫秒 -> 秒
            if (tk.length() < 8) return "服务器未返回 token";
            token = tk;
            expiresAt = exp > 0 ? exp : (System.currentTimeMillis() / 1000 + 30 * 86400L);
            sp.edit().putString("token", token).putLong("expiresAt", expiresAt).apply();
            return null;
        }
        String code = pick(resp, "code");
        String msg = pick(resp, "message");
        if (msg.length() == 0) msg = pick(resp, "msg");
        lastErr = (msg.length() > 0 ? msg : "激活失败") + (code.length() > 0 ? "（" + code + "）" : "");
        return lastErr;
    }

    /** 心跳续期（服务端接口就绪后生效；失败不影响本地宽限） */
    public void refresh(Context c) {
        if (!isValid()) return;
        String dev = deviceId(c);
        String body = "{\"token\":\"" + token + "\",\"deviceId\":\"" + dev + "\"}";
        String resp = post(BASE + "/api/v1/kernel/refresh", body);
        if (resp != null && resp.contains("\"ok\":true")) {
            String ex = pick(resp, "expiresAt");
            try {
                long exp = Long.parseLong(ex.replaceAll("[^0-9]", ""));
                if (exp > 100000000000L) exp = exp / 1000;
                if (exp > 0) {
                    expiresAt = exp;
                    sp.edit().putLong("expiresAt", exp).apply();
                }
            } catch (Throwable ignored) {
            }
        }
    }

    public void clear() {
        token = "";
        expiresAt = 0;
        sp.edit().remove("token").remove("expiresAt").apply();
    }

    // ---- 极简 JSON 取值（不引第三方库）----
    static String pick(String json, String key) {
        String k = "\"" + key + "\"";
        int i = json.indexOf(k);
        if (i < 0) return "";
        int c = json.indexOf(':', i + k.length());
        if (c < 0) return "";
        int p = c + 1;
        while (p < json.length() && Character.isWhitespace(json.charAt(p))) p++;
        if (p >= json.length()) return "";
        if (json.charAt(p) == '"') {
            int e = json.indexOf('"', p + 1);
            return e < 0 ? "" : json.substring(p + 1, e);
        }
        int e = p;
        while (e < json.length() && ",}\n\r".indexOf(json.charAt(e)) < 0) e++;
        return json.substring(p, e).trim();
    }

    static String post(String urlStr, String body) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(15000);
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            conn.setRequestProperty("User-Agent", "tpen-kernel/1.0");
            OutputStream os = conn.getOutputStream();
            os.write(body.getBytes("UTF-8"));
            os.flush();
            os.close();
            int code = conn.getResponseCode();
            java.io.InputStream is = (code >= 200 && code < 400) ? conn.getInputStream() : conn.getErrorStream();
            if (is == null) return null;
            BufferedReader br = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            StringBuilder sb = new StringBuilder();
            String ln;
            while ((ln = br.readLine()) != null) sb.append(ln);
            br.close();
            return sb.toString();
        } catch (Throwable t) {
            return null;
        } finally {
            if (conn != null) conn.disconnect();
        }
    }
}
