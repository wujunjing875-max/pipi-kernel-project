"""核心几何/数学：圆形 FOV 判定、像素↔角度换算、平滑与预判。"""
from __future__ import annotations

import math


def dist2(dx: float, dy: float) -> float:
    return dx * dx + dy * dy


def dist(dx: float, dy: float) -> float:
    return math.sqrt(dx * dx + dy * dy)


def in_circle(dx: float, dy: float, r: float) -> bool:
    """圆形 FOV 判定：整数半径内才算候选。绝不用矩形/欧氏切比雪夫混判。"""
    return dist2(dx, dy) <= r * r


def px_per_deg(hfov_deg: float, width: int) -> float:
    return float(width) / float(hfov_deg)


def px_to_deg(px: float, hfov_deg: float, width: int) -> float:
    return px / px_per_deg(hfov_deg, width)


def deg_to_px(deg: float, hfov_deg: float, width: int) -> float:
    return deg * px_per_deg(hfov_deg, width)


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else (hi if v > hi else v)


def clamp_vec(x: float, y: float, max_len: float) -> tuple[float, float]:
    m = math.hypot(x, y)
    if m > max_len and m > 1e-9:
        s = max_len / m
        return x * s, y * s
    return x, y


def smooth_delta(dx: float, dy: float, factor: float, max_step_px: float) -> tuple[float, float]:
    """两段式平滑：先指数缓动逼近，再对单帧最大位移做限幅（防瞬移特征）。"""
    ax, ay = dx * factor, dy * factor
    return clamp_vec(ax, ay, max_step_px)


def predict(px: float, py: float, vx: float, vy: float, ms: float) -> tuple[float, float]:
    """按目标速度做线性预判，补偿读取延迟 + 单帧渲染延迟。"""
    t = ms / 1000.0
    return px + vx * t, py + vy * t


def deadzone(dx: float, dy: float, eps: float) -> tuple[float, float]:
    if dist(dx, dy) <= eps:
        return 0.0, 0.0
    return dx, dy


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def ease_out_cubic(t: float) -> float:
    t = clamp(t, 0.0, 1.0)
    return 1.0 - (1.0 - t) ** 3
