"""一键跑通：启动内核层 → 自动拉起靶场 → 自瞄接管 → 出帧 / GIF / 指标报告。

    python run_test.py                 # 老样子，全自动
    python run_test.py --no-render     # 只跑数值，不渲染图片
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import statistics
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core import kernel
from core.backends import TcpBackend
from core.engine import AimbotConfig, AimbotEngine
from core.mathx import dist
from core.overlay import contact_sheet, render

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "out")

BANNER = r"""
   _  __                     _       _     _                  _   _
  | |/ /                    | |     / \   (_)                | | (_)
  | ' / ___ _ __ _ __   ___ | |    / _ \   _ _ __ ___   ___  | |_ _  ___  _ __
  |  < / _ \ '__| '_ \ / _ \| |   / ___ \ | | '_ ` _ \ / _ \ | __| |/ _ \| '_ \
  | . \  __/ |  | | | |  __/| |  / /   \_\| | | | | | |  __/ | |_| | (_) | | | |
  |_|\_\___|_|  |_| |_|\___|_|_| /_/      |_|_| |_| |_|\___|  \__|_|\___/|_| |_|
"""


def load_cfg(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as fp:
        return json.load(fp)


def wait_port(port: int, pid: int, timeout: float = 12.0) -> bool:
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=0.4)
            s.close()
            return True
        except OSError:
            time.sleep(0.15)
    return False


def launch_arena(cfg: dict) -> subprocess.Popen:
    g = cfg["game"]
    cmd = [sys.executable, os.path.join(ROOT, "game", "demo_game.py"),
           "--port", str(g["port"]), "--targets", str(g["targets"]), "--seed", str(g["seed"])]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                         encoding="utf-8", errors="replace")
    line = p.stdout.readline() if p.stdout else ""
    print(f"[launch] {line.strip() or '(no banner)'}")
    print(f"[launch] game pid = {p.pid}")
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=os.path.join(ROOT, "config.json"))
    ap.add_argument("--no-render", action="store_true")
    ap.add_argument("--frames", type=int, default=0)
    args = ap.parse_args()

    print(BANNER)
    cfg = load_cfg(args.config)
    a = cfg["aimbot"]
    r = cfg["render"]
    frames_total = args.frames or r["frames"]
    tpf = r["ticks_per_frame"]
    dt = tpf / r["tick_hz"]

    os.makedirs(OUT, exist_ok=True)

    # ---------- 1. 内核层 ----------
    print("[kernel] probing control device ...")
    st = kernel.probe()
    print(f"[kernel] mode={st['mode']}  win32_error={st['win32_error']}  {st['detail']}")
    print(f"[kernel] service: {kernel.service_state()}")
    if st["mode"] != "kernel":
        print("[kernel] 驱动设备未就绪 -> 自动降级 user 模式（OpenProcess + RPM），引擎逻辑不变")

    # ---------- 2. 自动启动游戏 ----------
    print("[game] auto launch arena ...")
    game = launch_arena(cfg)
    if not wait_port(cfg["game"]["port"], game.pid):
        print("[game] port not ready, abort")
        game.kill()
        return 2

    backend = TcpBackend(port=cfg["game"]["port"])
    hs = backend.handshake()
    print(f"[game] attached: {hs}")

    # ---------- 3. 自瞄接管 ----------
    acfg = AimbotConfig(**a)
    engine = AimbotEngine(acfg, cfg["screen"]["w"], cfg["screen"]["h"])
    print(f"[aim] fov=circle r={acfg.fov_radius}px  smooth={acfg.smooth}  pred={acfg.pred_ms}ms  "
          f"mode={acfg.target_mode}  trigger<={acfg.trigger_px}px")

    frames = []
    recs = []
    key_idx = {6, 26, 120, frames_total - 1}
    t0 = time.perf_counter()

    for i in range(frames_total):
        snap = backend.step(tpf)
        res = engine.step(snap["targets"], dt)
        recs.append(res)

        if not args.no_render and i in key_idx:
            meta = {
                "lock_id": res.lock_id,
                "trigger": res.trigger,
                "projected": res.projected,
                "aim_point": res.aim_point,
                "candidates": res.candidates,
                "post_err": res.post_err,
                "pre_err": res.pre_err,
                "kernel_mode": st["mode"],
                "backend": backend.name,
                "backend_detail": st["detail"][:28],
                "frame": i + 1,
                "total": frames_total,
                "status": f"tick {snap['tick']}  ·  {acfg.fov_shape} fov  ·  target={acfg.target_mode}",
            }
            frames.append(render(snap, engine, acfg, meta))

    # 全量录帧（GIF 用，隔帧取）
    if not args.no_render:
        engine2 = AimbotEngine(acfg, cfg["screen"]["w"], cfg["screen"]["h"])
        backend2 = TcpBackend(port=cfg["game"]["port"])
        # 重放同一段数据以生成 GIF，避免两次采样不一致
        gif_frames = []
        backend2._call({"cmd": "state"})
        for j in range(160):
            snap = backend2.step(tpf)
            res = engine2.step(snap["targets"], dt)
            if j % 2 or j < 12:
                meta = {
                    "lock_id": res.lock_id, "trigger": res.trigger, "projected": res.projected,
                    "aim_point": res.aim_point, "candidates": res.candidates,
                    "post_err": res.post_err, "pre_err": res.pre_err,
                    "kernel_mode": st["mode"], "backend": backend.name,
                    "backend_detail": st["detail"][:28],
                    "frame": j + 1, "total": 160,
                    "status": f"tick {snap['tick']}  ·  circle fov  ·  lock={res.lock_id}",
                }
                gif_frames.append(render(snap, engine2, acfg, meta).resize(
                    (cfg["screen"]["w"] // 2, cfg["screen"]["h"] // 2)))
        backend2.close()

    wall = time.perf_counter() - t0

    # ---------- 4. 指标 ----------
    locked = [x for x in recs if x.lock_id is not None]
    post = [x.post_err for x in locked if x.pre_err > 0]
    settled = [x.post_err for x in locked if x.post_err <= acfg.trigger_px]
    first = next((x for x in recs if x.lock_id is not None), None)
    within = [x for x in locked if x.post_err <= 8.0]
    metrics = {
        "frames": frames_total,
        "sim_duration_s": round(frames_total * dt, 3),
        "wall_time_s": round(wall, 3),
        "fov_shape": acfg.fov_shape,
        "fov_radius_px": acfg.fov_radius,
        "fov_area_px2": round(3.141592653589793 * acfg.fov_radius ** 2, 1),
        "lock_rate": round(len(locked) / frames_total * 100, 2),
        "lock_rate_after_first": round(
            len([x for x in recs if first and x.tick >= first.tick and x.lock_id is not None])
            / max(1, frames_total - (first.tick - 1)) * 100, 2) if first else 0.0,
        "frames_locked": len(locked),
        "target_switches": engine.switch_count,
        "first_lock_frame": (first.tick if first else None),
        "first_lock_ms": round(first.t * 1000, 1) if first else None,
        "mean_pre_err_px": round(statistics.fmean([x.pre_err for x in locked]), 2) if locked else None,
        "mean_post_err_px": round(statistics.fmean([x.post_err for x in locked]), 2) if locked else None,
        "median_post_err_px": round(statistics.median([x.post_err for x in locked]), 2) if locked else None,
        "max_post_err_px": round(max([x.post_err for x in locked]), 2) if locked else None,
        "settle_ratio_le_8px": round(len(within) / max(1, len(locked)) * 100, 2),
        "trigger_frames": sum(1 for x in recs if x.trigger),
        "trigger_ratio": round(sum(1 for x in recs if x.trigger) / frames_total * 100, 2),
        "mean_err_deg": round(engine.err_deg(statistics.fmean([x.post_err for x in locked]) if locked else 0), 4),
        "kernel_mode": st["mode"],
        "kernel_detail": st["detail"],
        "backend": backend.name,
    }

    print("\n=== metrics ===")
    for k, v in metrics.items():
        print(f"  {k:22s} {v}")

    with open(os.path.join(OUT, "metrics.json"), "w", encoding="utf-8") as fp:
        json.dump(metrics, fp, ensure_ascii=False, indent=2)

    # ---------- 5. 出图 ----------
    if not args.no_render:
        names = ["01_scan", "02_acquire", "03_locked", "04_steady"]
        for im, nm in zip(frames, names):
            im.save(os.path.join(OUT, f"{nm}.png"))
        contact_sheet(frames, os.path.join(OUT, "contact_sheet.png"), cols=2, scale=0.46)
        if gif_frames:
            gif_frames[0].save(
                os.path.join(OUT, "aimbot.gif"),
                save_all=True, append_images=gif_frames[1:], duration=60, loop=0, optimize=True,
            )
        print(f"[out] {len(frames)} png + contact_sheet.png + aimbot.gif -> {OUT}")

    # ---------- 6. 报告 ----------
    write_report(cfg, metrics, st)

    backend.close()
    game.kill()
    print("[game] arena stopped")
    return 0


def write_report(cfg: dict, m: dict, kst: dict) -> None:
    a = cfg["aimbot"]
    md = f"""# 方案 B · 自瞄联调报告

生成时间：{time.strftime('%Y-%m-%d %H:%M:%S')}

## 一句话结论

圆形 FOV（r={int(a['fov_radius'])}px）自瞄链路已跑通：{m['frames']} 帧内锁定率 **{m['lock_rate']}%**，
锁定后平均残差 **{m['mean_post_err_px']}px（{m['mean_err_deg']}°）**，
{m['settle_ratio_le_8px']}% 的锁定帧误差 ≤8px，开火判定帧 {m['trigger_frames']} 帧。

## 运行链路

| 环节 | 实现 | 实测状态 |
|---|---|---|
| 内核层 | 控制设备 `\\\\.\\JianShangAim` IOCTL | **{m['kernel_mode']}** — {m['kernel_detail']} |
| 数据源 | {m['backend']}（进程外 TCP，模拟跨进程读实体） | 已连接 |
| 自瞄 | circle FOV + 迟滞选靶 + 速度预判 + 指数平滑 | 运行中 |
| 游戏 | 自动拉起 `game/demo_game.py`（144Hz） | 已启动并接管 |

## 自瞄参数

| 参数 | 值 |
|---|---|
| FOV 形状 | 圆形（点到准星距离 ≤ r） |
| FOV 半径 | {a['fov_radius']} px，面积 {m['fov_area_px2']} px² |
| 选靶 | {a['target_mode']}，切换迟滞 ×{a['switch_hysteresis']} |
| 平滑系数 | {a['smooth']} |
| 单帧限速 | {a['max_step_px']} px |
| 预判 | {a['pred_ms']} ms |
| 死区 | {a['deadzone_px']} px |
| 反应延迟 | {a['reaction_ms']} ms |
| 开火阈值 | {a['trigger_px']} px |

## 指标

| 指标 | 数值 |
|---|---|
| 总帧数 / 仿真时长 | {m['frames']} 帧 / {m['sim_duration_s']} s |
| 锁定帧数 / 锁定率 | {m['frames_locked']} / {m['lock_rate']}% |
| 首次锁定 | 第 {m['first_lock_frame']} 帧（{m['first_lock_ms']} ms） |
| 目标切换次数 | {m['target_switches']} |
| 平均预置误差 | {m['mean_pre_err_px']} px |
| 平均残差 | {m['mean_post_err_px']} px |
| 残差中位数 | {m['median_post_err_px']} px |
| 最大残差 | {m['max_post_err_px']} px |
| ≤8px 占比 | {m['settle_ratio_le_8px']}% |
| 开火帧 / 占比 | {m['trigger_frames']} / {m['trigger_ratio']}% |
| 墙钟耗时 | {m['wall_time_s']} s |

## 产物

- `out/01_scan.png` 扫描态（目标未进圈）
- `out/02_acquire.png` 捕获瞬间（圈边锁定）
- `out/03_locked.png` 锁定收敛
- `out/04_steady.png` 稳态跟枪
- `out/contact_sheet.png` 四帧对照
- `out/aimbot.gif` 动态回放
- `out/metrics.json` 原始指标

## 接到真游戏

1. 按 `driver/README.md` 用 WDK 编译 `driver/JianShangAim.sys`，管理员加载 → `probe()` 返回 kernel 模式。
2. 填 `offsets.json`（模块名、实体管理器 RVA、链表指针 RVA、数量 RVA、stride、字段偏移）。
3. 把 `run_test.py` 里的 `TcpBackend` 换成 `ProcessBackend(KernelClient(), pid, offsets.json)`，引擎一行不改。
"""
    with open(os.path.join(OUT, "report.md"), "w", encoding="utf-8") as fp:
        fp.write(md)
    print(f"[out] report.md written")


if __name__ == "__main__":
    sys.exit(main())
