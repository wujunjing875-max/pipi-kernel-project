"""叠加层渲染：圆形 FOV 圈、锁定框、HUD、状态徽章。纯 PIL，不依赖任何外部资源。"""
from __future__ import annotations

import math
import os

from PIL import Image, ImageDraw, ImageFont

from .mathx import dist

BG_TOP = (10, 14, 19)
BG_BOT = (18, 24, 32)
GRID = (26, 36, 48)
GRID_MAJOR = (34, 46, 60)
CYAN = (34, 211, 238)
GREEN = (34, 197, 94)
RED = (239, 68, 68)
AMBER = (245, 158, 11)
DIM = (110, 128, 148)
TEXT = (226, 236, 245)
BODY = (148, 163, 184)

_FONT_DIR = "C:/Windows/Fonts"
_FCACHE: dict = {}


def _font(size: int, mono: bool = False) -> ImageFont.FreeTypeFont:
    key = (size, mono)
    if key in _FCACHE:
        return _FCACHE[key]
    cands = ["consola.ttf", "consolab.ttf"] if mono else ["msyh.ttc", "msyhbd.ttc", "simhei.ttf"]
    for c in cands:
        p = os.path.join(_FONT_DIR, c)
        if os.path.exists(p):
            try:
                f = ImageFont.truetype(p, size)
                _FCACHE[key] = f
                return f
            except Exception:
                continue
    f = ImageFont.load_default()
    _FCACHE[key] = f
    return f


def _bg(w: int, h: int) -> Image.Image:
    img = Image.new("RGB", (w, h), BG_TOP)
    d = ImageDraw.Draw(img)
    for y in range(h):
        k = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * k) for i in range(3)))
    for x in range(0, w, 64):
        d.line([(x, 0), (x, h)], fill=GRID)
    for y in range(0, h, 64):
        d.line([(0, y), (w, y)], fill=GRID)
    for x in range(0, w, 256):
        d.line([(x, 0), (x, h)], fill=GRID_MAJOR)
    for y in range(0, h, 256):
        d.line([(0, y), (w, y)], fill=GRID_MAJOR)
    return img


def _dashed_circle(d: ImageDraw.ImageDraw, cx: float, cy: float, r: float, color, width: int = 2, dash: int = 16, gap: int = 12):
    seg = 2.0
    a = 0.0
    step = 360.0 / max(1, int(360 / seg))
    on = True
    acc = 0.0
    while a < 360.0:
        if on:
            d.arc([cx - r, cy - r, cx + r, cy + r], a, a + step, fill=color, width=width)
        acc += step
        if on and acc >= dash * 0.35:
            on, acc = False, 0.0
        elif not on and acc >= gap * 0.35:
            on, acc = True, 0.0
        a += step


def _target(d, t, locked: bool, aim_point, trigger: bool):
    sx, sy = t["sx"], t["sy"]
    w, h = 20, 46
    col = GREEN if locked else BODY
    # 身体
    d.rounded_rectangle([sx - w / 2, sy - h / 2, sx + w / 2, sy + h / 2], radius=7, outline=col, width=2)
    # 头
    d.ellipse([sx - 9, sy + t["head_off"] - 9, sx + 9, sy + t["head_off"] + 9], outline=col, width=2)
    # 血条
    hpw = 26
    hp = max(0, min(100, t["hp"]))
    top = sy + t["head_off"] - 20
    d.rectangle([sx - hpw / 2, top, sx + hpw / 2, top + 4], outline=DIM, width=1)
    d.rectangle([sx - hpw / 2 + 1, top + 1, sx - hpw / 2 + 1 + (hpw - 2) * hp / 100, top + 3], fill=(220, 60, 60))
    if locked:
        d.text((sx + 14, top - 4), f"#{t['id']}", font=_font(13, True), fill=GREEN)
        if aim_point:
            ax, ay = aim_point
            s = 7
            c = RED if trigger else AMBER
            d.line([(ax - s, ay), (ax + s, ay)], fill=c, width=2)
            d.line([(ax, ay - s), (ax, ay + s)], fill=c, width=2)
            d.ellipse([ax - s - 3, ay - s - 3, ax + s + 3, ay + s + 3], outline=c, width=1)


def _crosshair(d, cx, cy, trigger: bool):
    c = RED if trigger else TEXT
    d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=c)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        d.line([(cx + dx * 7, cy + dy * 7), (cx + dx * 20, cy + dy * 20)], fill=c, width=2)


def _panel(d, xy, lines, title=None, width=300):
    x, y = xy
    f = _font(15)
    fm = _font(14, True)
    lh = 22
    h = lh * len(lines) + 22 + (24 if title else 0)
    d.rounded_rectangle([x, y, x + width, y + h], radius=8, fill=(13, 18, 25), outline=(40, 54, 70), width=1)
    ty = y + 10
    if title:
        d.text((x + 12, ty), title, font=_font(14), fill=CYAN)
        ty += 24
    for k, v in lines:
        d.text((x + 12, ty), k, font=f, fill=DIM)
        d.text((x + 118, ty), str(v), font=fm, fill=TEXT)
        ty += lh
    return h


def render(state: dict, engine, cfg, meta: dict) -> Image.Image:
    world = state.get("world", {"w": 1280, "h": 720})
    W, H = int(world["w"]), int(world["h"])
    img = _bg(W, H)
    d = ImageDraw.Draw(img)
    cx, cy = W / 2.0, H / 2.0

    lock_id = meta.get("lock_id")
    trigger = meta.get("trigger", False)
    proj = meta.get("projected") or []

    # 锁定目标到准星的连线（在目标下层）
    for t in proj:
        locked = (t["id"] == lock_id)
        d_ = dist(t["dx"], t["dy"])
        if not locked and d_ > cfg.fov_radius:
            d.line([(cx, cy), (t["sx"], t["sy"])], fill=(30, 42, 56), width=1)

    for t in proj:
        _target(d, t, t["id"] == lock_id, meta.get("aim_point"), trigger)

    # 圆形 FOV 圈
    ring = GREEN if lock_id is not None else CYAN
    _dashed_circle(d, cx, cy, cfg.fov_radius, ring, 2)
    d.ellipse([cx - cfg.fov_radius - 3, cy - 3, cx - cfg.fov_radius + 3, cy + 3], fill=ring)
    d.ellipse([cx + cfg.fov_radius - 3, cy - 3, cx + cfg.fov_radius + 3, cy + 3], fill=ring)
    d.ellipse([cx - 3, cy - cfg.fov_radius - 3, cx + 3, cy - cfg.fov_radius + 3], fill=ring)
    d.ellipse([cx - 3, cy + cfg.fov_radius - 3, cx + 3, cy + cfg.fov_radius + 3], fill=ring)
    d.text((cx + cfg.fov_radius + 12, cy - 8), f"FOV r={int(cfg.fov_radius)}px · circle", font=_font(13), fill=ring)

    _crosshair(d, cx, cy, trigger)

    # 左上 HUD
    pre = meta.get("pre_err", 0.0)
    post = meta.get("post_err", 0.0)
    lines = [
        ("FOV 形状", "圆形 (circle)"),
        ("FOV 半径", f"{int(cfg.fov_radius)} px"),
        ("锁定目标", f"#{lock_id}" if lock_id is not None else "无"),
        ("圈内候选", meta.get("candidates", 0)),
        ("瞄准误差", f"{post:.2f} px / {engine.err_deg(post):.3f}°"),
        ("预判补偿", f"{cfg.pred_ms:.0f} ms"),
        ("平滑 / 限速", f"{cfg.smooth:.2f} / {cfg.max_step_px:.0f}px"),
    ]
    _panel(d, (18, 16), lines, title="AIMBOT · 运行中", width=290)

    # 右上状态
    badge = meta.get("kernel_mode", "user")
    bcol = GREEN if badge == "kernel" else AMBER
    bx, by = W - 300, 16
    d.rounded_rectangle([bx, by, W - 18, by + 96], radius=8, fill=(13, 18, 25), outline=(40, 54, 70), width=1)
    d.text((bx + 12, by + 10), "KERNEL", font=_font(13), fill=DIM)
    d.text((bx + 92, by + 10), badge.upper(), font=_font(14, True), fill=bcol)
    d.text((bx + 12, by + 34), "backend", font=_font(13), fill=DIM)
    d.text((bx + 92, by + 34), meta.get("backend", "-")[:22], font=_font(13, True), fill=TEXT)
    d.text((bx + 12, by + 58), "frame", font=_font(13), fill=DIM)
    d.text((bx + 92, by + 58), f"{meta.get('frame', 0)}/{meta.get('total', 0)}", font=_font(13, True), fill=TEXT)
    d.ellipse([bx + 12, by + 78, bx + 20, by + 86], fill=bcol)
    d.text((bx + 26, by + 74), meta.get("backend_detail", ""), font=_font(12), fill=DIM)

    # 底部进度条 + 状态
    bar_y = H - 46
    d.rounded_rectangle([18, bar_y, W - 18, bar_y + 28], radius=6, fill=(13, 18, 25), outline=(40, 54, 70), width=1)
    total = max(1, meta.get("total", 1))
    prog = meta.get("frame", 0) / total
    d.rectangle([20, bar_y + 2, 20 + (W - 40) * prog, bar_y + 26], fill=(22, 40, 52))
    msg = meta.get("status", "")
    d.text((30, bar_y + 5), msg, font=_font(14), fill=TEXT)
    tag = "TRIGGER" if trigger else ("LOCK" if lock_id is not None else "SCAN")
    tcol = RED if trigger else (GREEN if lock_id is not None else CYAN)
    d.text((W - 130, bar_y + 5), tag, font=_font(14, True), fill=tcol)

    return img


def contact_sheet(images, out_path: str, cols: int = 3, scale: float = 0.5):
    if not images:
        return
    tw, th = int(images[0].width * scale), int(images[0].height * scale)
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (tw * cols + 8 * (cols + 1), th * rows + 8 * (rows + 1)), (6, 9, 12))
    for i, im in enumerate(images):
        r, c = divmod(i, cols)
        sheet.paste(im.resize((tw, th), Image.LANCZOS), (8 + c * (tw + 8), 8 + r * (th + 8)))
    sheet.save(out_path)
