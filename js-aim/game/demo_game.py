"""Demo 靶场：进程外的 144Hz 实体模拟器。

启动后监听 127.0.0.1:27655，按客户端指令推进世界并返回实体快照。
由 run_test.py 自动拉起（Popen），也可以单独执行看它活着：
    python game/demo_game.py --port 27655
"""
from __future__ import annotations

import argparse
import json
import math
import random
import socketserver
import sys

W, H = 1280, 720
DT = 1.0 / 144.0
BOUND_X, BOUND_Y = 780.0, 440.0


class Sim:
    def __init__(self, count: int = 6, seed: int = 20261026):
        self.rng = random.Random(seed)
        self.tick = 0
        self.targets = [self._spawn(i) for i in range(count)]

    def _spawn(self, i: int) -> dict:
        r = self.rng
        return {
            "id": i,
            "x": r.uniform(-BOUND_X, BOUND_X),
            "y": r.uniform(-BOUND_Y, BOUND_Y),
            "vx": r.choice([-1, 1]) * r.uniform(90.0, 300.0),
            "vy": r.choice([-1, 1]) * r.uniform(40.0, 170.0),
            "phase": r.uniform(0.0, math.tau),
            "freq": r.uniform(1.4, 3.6),
            "hp": r.randint(40, 100),
            "head_off": -30.0,
            "alive": 1,
        }

    def step(self, n: int = 1) -> None:
        for _ in range(n):
            self.tick += 1
            for t in self.targets:
                t["phase"] += t["freq"] * DT
                vx = t["vx"] + math.sin(t["phase"]) * 70.0
                vy = t["vy"] + math.cos(t["phase"] * 0.7) * 45.0
                x = t["x"] + vx * DT
                y = t["y"] + vy * DT
                if x > BOUND_X or x < -BOUND_X:
                    t["vx"] *= -1
                    x = max(-BOUND_X, min(BOUND_X, x))
                if y > BOUND_Y or y < -BOUND_Y:
                    t["vy"] *= -1
                    y = max(-BOUND_Y, min(BOUND_Y, y))
                t["x"], t["y"] = x, y
                # 真实速度回写，供自瞄预判使用
                t["rvx"], t["rvy"] = vx, vy

    def snapshot(self) -> dict:
        out = []
        for t in self.targets:
            out.append(
                {
                    "id": t["id"],
                    "x": round(t["x"], 3),
                    "y": round(t["y"], 3),
                    "vx": round(t.get("rvx", t["vx"]), 3),
                    "vy": round(t.get("rvy", t["vy"]), 3),
                    "hp": t["hp"],
                    "head_off": t["head_off"],
                    "alive": bool(t["alive"]),
                }
            )
        return {"tick": self.tick, "targets": out, "world": {"w": W, "h": H}}


class Handler(socketserver.StreamRequestHandler):
    sim: Sim = None  # injected

    def handle(self) -> None:
        while True:
            line = self.rfile.readline()
            if not line:
                return
            try:
                req = json.loads(line)
            except json.JSONDecodeError:
                self._send({"error": "bad json"})
                continue
            cmd = req.get("cmd", "")
            if cmd == "ping":
                self._send({"ok": True, "pid": __import__("os").getpid(), "tick": self.sim.tick})
            elif cmd == "step":
                self.sim.step(int(req.get("n", 1)))
                self._send(self.sim.snapshot())
            elif cmd == "state":
                self._send(self.sim.snapshot())
            elif cmd == "quit":
                self._send({"ok": True})
                return
            else:
                self._send({"error": f"unknown cmd {cmd}"})

    def _send(self, obj: dict) -> None:
        self.wfile.write((json.dumps(obj) + "\n").encode())
        self.wfile.flush()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=27655)
    ap.add_argument("--targets", type=int, default=6)
    ap.add_argument("--seed", type=int, default=20261026)
    args = ap.parse_args()

    Handler.sim = Sim(args.targets, args.seed)
    srv = Server(("127.0.0.1", args.port), Handler)
    print(f"[arena] listening 127.0.0.1:{args.port} targets={args.targets} seed={args.seed}", flush=True)
    try:
        srv.serve_forever(poll_interval=0.05)
    except KeyboardInterrupt:
        pass
    finally:
        srv.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
