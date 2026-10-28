"""自瞄引擎：候选筛选(圆形FOV) → 目标选择(带切换迟滞) → 预判 → 平滑 → 输出视角增量。

坐标约定（与真实引擎投影一致）：
    屏幕坐标 = 准星中心 + (世界坐标 - 相机视角)
    要让目标落到准星，需要 视角 += (目标屏幕坐标 - 准星中心)
"""
from __future__ import annotations

from dataclasses import dataclass, field

from .mathx import (
    clamp_vec,
    deadzone,
    dist,
    in_circle,
    px_to_deg,
    predict,
    smooth_delta,
)

AIM_KEYS = ("id", "x", "y", "vx", "vy", "hp", "head_off", "alive")


@dataclass
class AimbotConfig:
    fov_shape: str = "circle"
    fov_radius: float = 210.0          # 圆形 FOV 半径（像素）
    hfov_deg: float = 90.0             # 用于把像素误差换算成角度误差
    smooth: float = 0.45               # 指数平滑系数，1.0 = 硬锁
    max_step_px: float = 90.0          # 单帧最大位移上限，防瞬移特征
    pred_ms: float = 22.0              # 预判补偿时长
    deadzone_px: float = 1.2           # 死区，抑制抖动
    reaction_ms: float = 90.0          # 换目标后的反应延迟
    trigger_px: float = 6.0            # 误差小于该值判定为可开火
    target_mode: str = "nearest"       # nearest / nearest_head / lowest_hp
    head_bias: float = 0.45            # 0=躯干 1=头部
    switch_hysteresis: float = 1.30    # 新目标需近 30% 才抢锁


@dataclass
class AimResult:
    tick: int = 0
    t: float = 0.0
    view: tuple[float, float] = (0.0, 0.0)
    lock_id: int | None = None
    candidates: int = 0
    pre_dx: float = 0.0
    pre_dy: float = 0.0
    pre_err: float = 0.0
    applied_x: float = 0.0
    applied_y: float = 0.0
    post_err: float = 0.0
    in_fov: bool = False
    trigger: bool = False
    aim_point: tuple[float, float] | None = None
    projected: list = field(default_factory=list)


class AimbotEngine:
    def __init__(self, cfg: AimbotConfig, screen_w: int, screen_h: int):
        self.cfg = cfg
        self.cx = screen_w / 2.0
        self.cy = screen_h / 2.0
        self.view = [0.0, 0.0]
        self.tick = 0
        self.t = 0.0
        self.lock_id: int | None = None
        self.lock_tick: int | None = None
        self.lock_age_ms = 0.0
        self.switch_count = 0
        self.first_lock_tick: int | None = None

    # ---------- 投影 ----------
    def project(self, targets, view=None):
        vx, vy = self.view if view is None else view
        out = []
        for t in targets:
            if not t.get("alive", True):
                continue
            sx = self.cx + (t["x"] - vx)
            sy = self.cy + (t["y"] - vy)
            out.append(
                {
                    "id": t["id"],
                    "sx": sx,
                    "sy": sy,
                    "dx": sx - self.cx,
                    "dy": sy - self.cy,
                    "hp": t.get("hp", 100),
                    "vx": t.get("vx", 0.0),
                    "vy": t.get("vy", 0.0),
                    "head_off": t.get("head_off", -30.0),
                }
            )
        return out

    # ---------- 目标选择 ----------
    def _score(self, c) -> float:
        mode = self.cfg.target_mode
        if mode == "nearest_head":
            hy = c["sy"] + c["head_off"] - self.cy
            return dist(c["dx"], hy)
        if mode == "lowest_hp":
            return c["hp"] + dist(c["dx"], c["dy"]) * 0.05
        return dist(c["dx"], c["dy"])

    def _select(self, cands):
        if not cands:
            return None
        best = min(cands, key=self._score)
        # 切换迟滞：当前锁定目标只要还在圈内且不太远，就不换，避免来回抽搐
        if self.lock_id is not None:
            cur = next((c for c in cands if c["id"] == self.lock_id), None)
            if cur is not None and self._score(cur) <= self._score(best) * self.cfg.switch_hysteresis:
                return cur
            if cur is not None and cur is not best:
                self.switch_count += 1
        elif best is not None:
            self.first_lock_tick = self.tick
        return best

    # ---------- 主循环一步 ----------
    def step(self, targets, dt: float) -> AimResult:
        cfg = self.cfg
        self.tick += 1
        self.t += dt

        proj = self.project(targets)
        cands = [c for c in proj if in_circle(c["dx"], c["dy"], cfg.fov_radius)]

        prev_lock = self.lock_id
        tgt = self._select(cands)
        self.lock_id = tgt["id"] if tgt else None

        if self.lock_id is not None and self.lock_id == prev_lock:
            self.lock_age_ms += dt * 1000.0
        else:
            self.lock_age_ms = 0.0
            if self.lock_id is not None and prev_lock is None:
                pass

        res = AimResult(tick=self.tick, t=self.t, candidates=len(cands), projected=proj)
        res.lock_id = self.lock_id
        res.in_fov = tgt is not None

        if tgt is None:
            res.view = (self.view[0], self.view[1])
            return res

        # 瞄准点：躯干中心 ↔ 头部 按 head_bias 插值
        hy = tgt["sy"] + tgt["head_off"] * cfg.head_bias
        ax, ay = tgt["sx"], hy
        # 预判
        px_, py_ = predict(ax, ay, tgt["vx"], tgt["vy"], cfg.pred_ms)
        ddx, ddy = px_ - self.cx, py_ - self.cy
        res.aim_point = (ax, ay)
        res.pre_dx, res.pre_dy = ddx, ddy
        res.pre_err = dist(ddx, ddy)

        # 反应延迟内不动手（模拟人类反应窗口）
        if self.lock_age_ms < cfg.reaction_ms:
            res.view = (self.view[0], self.view[1])
            res.post_err = res.pre_err
            return res

        ddx, ddy = deadzone(ddx, ddy, cfg.deadzone_px)
        mvx, mvy = smooth_delta(ddx, ddy, cfg.smooth, cfg.max_step_px)
        # 硬限幅二次保险
        mvx, mvy = clamp_vec(mvx, mvy, cfg.max_step_px)
        self.view[0] += mvx
        self.view[1] += mvy

        # 移动后的真实残差（用同一帧世界状态重投影）
        after = self.project(targets)
        me = next((c for c in after if c["id"] == self.lock_id), None)
        if me is not None:
            ahy = me["sy"] + me["head_off"] * cfg.head_bias
            res.post_err = dist(me["sx"] - self.cx, ahy - self.cy)
            res.in_fov = in_circle(me["sx"] - self.cx, me["sy"] - self.cy, cfg.fov_radius)
        else:
            res.post_err = res.pre_err

        res.applied_x, res.applied_y = mvx, mvy
        res.trigger = res.post_err <= cfg.trigger_px
        res.view = (self.view[0], self.view[1])
        return res

    def err_deg(self, px: float) -> float:
        return px_to_deg(px, self.cfg.hfov_deg, int(self.cx * 2))
