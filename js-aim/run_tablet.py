"""一键把自瞄叠加层推到平板（实时绘制）+ 出验证截图。

    python run_tablet.py --port 8788 --frames 240

流程：探内核 → 自动起靶场 → 自瞄接管 → MJPEG 推流 → adb reverse → 平板浏览器全屏打开
     → screencap 抓平板实拍证明 → 生成离线回放并 push 到平板
"""
from __future__ import annotations

import argparse
import base64
import io
import json
import os
import re
import statistics
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image

from core import kernel
from core.backends import TcpBackend
from core.engine import AimbotConfig, AimbotEngine
from core.overlay import render
from core.stream import Hub, encode_jpeg, serve

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "out")


def adb(*args: str, timeout: int = 25) -> subprocess.CompletedProcess:
    return subprocess.run(["adb", *args], capture_output=True, timeout=timeout)


def adb_text(*args: str) -> str:
    p = adb(*args)
    return (p.stdout or b"").decode("utf-8", "replace").strip()


def pick_device() -> str | None:
    out = adb_text("devices", "-l")
    for line in out.splitlines()[1:]:
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "device":
            return parts[0]
    return None


def tablet_landscape() -> tuple[int, int]:
    out = adb_text("shell", "wm", "size")
    m = re.search(r"(\d+)x(\d+)", out)
    if not m:
        return 3000, 2120
    a, b = int(m.group(1)), int(m.group(2))
    return max(a, b), min(a, b)


def screencap(path: str) -> bool:
    remote = "/sdcard/js_cap.png"
    if adb("shell", "screencap", "-p", remote).returncode != 0:
        return False
    if adb("pull", remote, path).returncode != 0:
        return False
    adb("shell", "rm", "-f", remote)
    return os.path.exists(path) and os.path.getsize(path) > 1000


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8788)
    ap.add_argument("--frames", type=int, default=240)
    ap.add_argument("--fps", type=float, default=30.0)
    ap.add_argument("--config", default=os.path.join(ROOT, "config.json"))
    ap.add_argument("--no-launch", action="store_true", help="只在平板打开页面，不推流")
    args = ap.parse_args()

    os.makedirs(OUT, exist_ok=True)
    with open(args.config, "r", encoding="utf-8") as fp:
        cfg = json.load(fp)

    # ---------- 0. 设备 ----------
    dev = pick_device()
    if not dev:
        print("[adb] 未发现已授权设备，先插好线并在平板上点『允许 USB 调试』")
        print(adb_text("devices", "-l"))
        return 2
    model = adb_text("shell", "getprop", "ro.product.model")
    rel = adb_text("shell", "getprop", "ro.build.version.release")
    pw, ph = tablet_landscape()
    print(f"[adb] device={dev} model={model} android={rel} landscape={pw}x{ph}")

    # 唤醒 + 常亮，避免中途黑屏
    adb("shell", "input", "keyevent", "KEYCODE_WAKEUP")
    adb("shell", "wm", "dismiss-keyguard")
    adb("shell", "settings", "put", "system", "screen_off_timeout", "1800000")
    adb("shell", "svc", "power", "stayon", "true")

    # ---------- 1. 内核层 ----------
    st = kernel.probe()
    print(f"[kernel] mode={st['mode']} err={st['win32_error']} {st['detail']}")
    print(f"[kernel] service: {kernel.service_state()}")

    # ---------- 2. 起游戏 ----------
    g = cfg["game"]
    cmd = [sys.executable, os.path.join(ROOT, "game", "demo_game.py"),
           "--port", str(g["port"]), "--targets", str(g["targets"]), "--seed", str(g["seed"])]
    game = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                            encoding="utf-8", errors="replace")
    print(f"[game] {game.stdout.readline().strip()}  pid={game.pid}")
    t0 = time.time()
    backend = None
    while time.time() - t0 < 12:
        try:
            backend = TcpBackend(port=g["port"])
            break
        except OSError:
            time.sleep(0.15)
    if backend is None:
        print("[game] 连接失败")
        game.kill()
        return 3
    print(f"[game] attached {backend.handshake()}")

    # ---------- 3. 渲染尺寸按平板比例 ----------
    RW = 1280
    RH = int(round(RW * ph / pw / 2) * 2)
    acfg = AimbotConfig(**cfg["aimbot"])
    engine = AimbotEngine(acfg, RW, RH)
    print(f"[aim] circle fov r={acfg.fov_radius}px · render {RW}x{RH} (平板比例 {pw}x{ph})")

    # ---------- 4. 推流 ----------
    hub = Hub()
    srv = serve(hub, port=args.port)
    print(f"[stream] http://0.0.0.0:{args.port}/  (MJPEG)")
    r = adb("reverse", f"tcp:{args.port}", f"tcp:{args.port}")
    print(f"[adb] reverse -> {(r.stdout or r.stderr or b'').decode('utf-8','replace').strip() or 'ok'}")

    if not args.no_launch:
        adb("shell", "am", "start", "-a", "android.intent.action.VIEW",
            "-d", f"http://127.0.0.1:{args.port}/")
        print("[adb] 平板浏览器已拉起页面")

    # ---------- 5. 主循环：推 240 帧 ----------
    tpf = 5                      # 每帧推进 5 个 144Hz tick ≈ 34.7ms
    dt = tpf / 144.0
    keep = []
    recs = []
    shots = hits = 0
    last_shot = -1e9
    frame_dt = 1.0 / args.fps
    t_start = time.perf_counter()

    for i in range(args.frames):
        snap = backend.step(tpf)
        res = engine.step(snap["targets"], dt)
        recs.append(res)

        if res.trigger and (time.perf_counter() - last_shot) >= 0.1:
            last_shot = time.perf_counter()
            shots += 1
            if res.post_err <= 12.0:
                hits += 1

        img = render(snap, engine, acfg, {
            "lock_id": res.lock_id, "trigger": res.trigger, "projected": res.projected,
            "aim_point": res.aim_point, "candidates": res.candidates,
            "post_err": res.post_err, "pre_err": res.pre_err,
            "kernel_mode": st["mode"], "backend": "tablet/stream",
            "backend_detail": f"{model} · MJPEG", "frame": i + 1, "total": args.frames,
            "status": f"tick {snap['tick']} · circle fov · live on tablet",
        })
        hub.push(encode_jpeg(img, 76), {
            "lock_id": res.lock_id, "post_err": res.post_err,
            "kernel_mode": st["mode"], "frame": i + 1, "trigger": res.trigger,
        })
        if i % 2 == 0:
            keep.append(img.resize((RW // 2, RH // 2), Image.LANCZOS))

        sleep = frame_dt - ((time.perf_counter() - t_start) - i * frame_dt)
        if sleep > 0:
            time.sleep(sleep)

    live_s = time.perf_counter() - t_start
    print(f"[stream] {args.frames} 帧推完，{live_s:.1f}s，客户端数={hub.clients}")

    # ---------- 6. 平板实拍证明 ----------
    time.sleep(1.2)
    proof = os.path.join(OUT, "tablet_live.png")
    ok = screencap(proof)
    print(f"[proof] screencap -> {proof} ({'ok' if ok else 'failed'})")

    # ---------- 7. 离线回放（进程结束后仍能看） ----------
    gif_path = os.path.join(OUT, "tablet_live.gif")
    if keep:
        keep[0].save(gif_path, save_all=True, append_images=keep[1:], duration=int(1000 / args.fps * 2),
                     loop=0, optimize=True)
        adb("push", gif_path, "/sdcard/Download/jsaim_live.gif")
        print(f"[push] {gif_path} -> /sdcard/Download/jsaim_live.gif")

    # ---------- 8. 指标 ----------
    locked = [x for x in recs if x.lock_id is not None]
    post = [x.post_err for x in locked]
    first = next((x for x in recs if x.lock_id is not None), None)
    m = {
        "device": f"{model} / android {rel}",
        "render": f"{RW}x{RH}",
        "frames": len(recs),
        "live_seconds": round(live_s, 2),
        "kernel_mode": st["mode"],
        "kernel_detail": st["detail"],
        "fov": f"circle r={acfg.fov_radius}px",
        "lock_rate": round(len(locked) / len(recs) * 100, 2),
        "first_lock_ms": round(first.t * 1000, 1) if first else None,
        "mean_post_err_px": round(statistics.fmean(post), 2) if post else None,
        "median_post_err_px": round(statistics.median(post), 2) if post else None,
        "max_post_err_px": round(max(post), 2) if post else None,
        "shots": shots,
        "hits": hits,
        "hit_rate": round(hits / shots * 100, 2) if shots else 0.0,
        "trigger_ratio": round(sum(1 for x in recs if x.trigger) / len(recs) * 100, 2),
        "switches": engine.switch_count,
    }
    print("\n=== tablet metrics ===")
    for k, v in m.items():
        print(f"  {k:20s} {v}")
    with open(os.path.join(OUT, "tablet_metrics.json"), "w", encoding="utf-8") as fp:
        json.dump(m, fp, ensure_ascii=False, indent=2)

    # 留着推流，方便用户看一眼；退出时自动收尾
    print("[done] 推流仍在运行中（本回合结束自动停止），离线回放已推到平板")
    backend.close()
    game.kill()
    srv.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
