# -*- coding: utf-8 -*-
"""修自瞄按钮文字：用 glyph 图集画中文「自瞄/锁定」，并支持缩放"""
import io, os, sys

P = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src/Java/Ov50.java")
s = io.open(P, encoding="utf-8").read()

# 1) 新增 gtextS（可缩放图集文字），插在 gtext 之后
anchor = """            if (mSetCF != null) mSetCF.invoke(paint, new Object[]{null});
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }

    // ---------- 物资中文名表 ----------"""
new = """            if (mSetCF != null) mSetCF.invoke(paint, new Object[]{null});
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }

    // 图集文字（可缩放，sc=1 原尺寸）：自瞄按钮用
    static void gtextS(Object c, String s, int x, int y, int color, int sc) {
        if (glyphBmp == null || s == null) return;
        if (sc < 1) sc = 1;
        try {
            mSetColor.invoke(paint, color);
            mSetAA.invoke(paint, true);
            Object cf = cfOf(color);
            if (cf != null && mSetCF != null) mSetCF.invoke(paint, cf);
            for (int i = 0; i < s.length(); i++) {
                int[] g = glyphR.get(s.charAt(i));
                if (g == null) continue;
                int dw = ((g[4] < 20) ? (g[4] + 2) : g[2]) * sc;
                Object sr = cRect.newInstance(g[0], g[1], g[0] + g[2], g[1] + g[3]);
                Object dr = cRect.newInstance(x, y, x + dw, y + g[3] * sc);
                mDrawBmp.invoke(c, glyphBmp, sr, dr, paint);
                x += g[4] * sc;
            }
            if (mSetCF != null) mSetCF.invoke(paint, new Object[]{null});
            mSetAA.invoke(paint, false);
        } catch (Throwable t) {}
    }

    // ---------- 物资中文名表 ----------"""
assert anchor in s, "gtext 锚点没找到"
s = s.replace(anchor, new, 1)

# 2) drawAim 文字段替换
old_txt = """        if (tfCN != null) {
            textCN(c, aimOn ? "锁定" : "自瞄", bx + bs * 0.17f, by + bs * 0.62f, bs * 0.30f, fg);
        } else if (glyphBmp == null) {
            gtext(c, aimOn ? "ON" : "AIM", bx + (int) (bs * 0.22f), by + (int) (bs * 0.40f), fg);
        } else {
            gtext(c, aimOn ? "ON" : "AI", bx + (int) (bs * 0.30f), by + (int) (bs * 0.40f), fg);
        }"""
new_txt = """        String lbl2 = aimOn ? "锁定" : "自瞄";
        int sc2 = bs / 95;
        if (sc2 < 1) sc2 = 1;
        int gw2 = gwidth(lbl2) * sc2;
        gtextS(c, lbl2, bx + (bs - gw2) / 2, by + (bs - 34 * sc2) / 2, fg, sc2);"""
assert old_txt in s, "drawAim 文字段没找到"
s = s.replace(old_txt, new_txt, 1)

# 3) 按钮配色更醒目
s = s.replace("""        int bg = aimOn ? 0xE600C853 : 0xD91B1F24;
        int fg = aimOn ? 0xFF06281A : 0xFFAEB9C2;""",
"""        int bg = aimOn ? 0xF200C853 : 0xE62A3038;
        int fg = aimOn ? 0xFF06281A : 0xFFE8EEF4;""", 1)

# 4) gvalue 里加"自瞄"字库兜底：若图集缺字，日志提示
s = s.replace("""        if (aimOn && aimHitCount > 0) {
            text(c, "" + aimHitCount, bx + bs - 24, by + 8, 3, 0xFFFFD54F);
        }""",
"""        if (aimOn && aimHitCount > 0) {
            text(c, "" + aimHitCount, bx + bs - 24, by + 8, 3, 0xFFFFD54F);
        }
        // 圈内敌人 x 距离（调试用，开自瞄才显示）
        if (aimOn && aimDbgTxt != null) {
            gtext(c, aimDbgTxt, bx + 6, by + bs + 6, 0xFFB9F6CA);
        }""", 1)

# 5) 调试文本变量
s = s.replace("""    static volatile int aimHitCount = 0;                     // 本帧圈内目标数""",
"""    static volatile int aimHitCount = 0;                     // 本帧圈内目标数
    static volatile String aimDbgTxt = null;                 // 圈内最优目标信息""", 1)

# 6) aimThread 里填充调试文本
s = s.replace("""                        if (best < 0) { aimRelease(); continue; }
                        float dx = txtX[best] - aimBtnX, dy = txtY[best] - aimBtnY;""",
"""                        if (best < 0) { aimRelease(); aimDbgTxt = null; continue; }
                        aimDbgTxt = txtDm[best] + "m";
                        float dx = txtX[best] - aimBtnX, dy = txtY[best] - aimBtnY;""", 1)

io.open(P, "w", encoding="utf-8", newline="\n").write(s)
print("patch2 ok")
