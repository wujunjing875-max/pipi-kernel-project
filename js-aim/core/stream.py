"""把叠加层实时推给平板：MJPEG over HTTP + 页面。

平板通过 `adb reverse tcp:8788 tcp:8788` 访问 http://127.0.0.1:8788/，
浏览器里就是全屏的绘制画面（不装任何 App）。
"""
from __future__ import annotations

import io
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = """<!doctype html>
<html lang="zh-CN"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#000000">
<title>JianShang Aim · Live</title>
<style>
  html,body{margin:0;padding:0;background:#000;height:100%;overflow:hidden;
    font-family:-apple-system,"HarmonyOS Sans","Noto Sans SC",sans-serif;-webkit-user-select:none}
  #wrap{position:fixed;inset:0;display:flex;align-items:center;justify-content:center}
  #feed{max-width:100%;max-height:100%;width:100%;height:100%;object-fit:contain;background:#000}
  #bar{position:fixed;left:0;right:0;top:0;padding:10px 16px;display:flex;gap:14px;align-items:center;
    font-size:13px;color:#9fb3c8;background:linear-gradient(#000c,#0000);pointer-events:none}
  .dot{width:8px;height:8px;border-radius:50%;background:#ef4444;box-shadow:0 0 8px #ef4444}
  .ok{background:#22c55e;box-shadow:0 0 8px #22c55e}
  #tip{position:fixed;left:0;right:0;bottom:0;padding:8px 16px;font-size:12px;color:#5b6b7d;text-align:center}
</style></head>
<body>
<div id="bar"><span class="dot" id="dot"></span><span id="st">连接中…</span><span id="k">kernel: ?</span><span id="f">fps: ?</span></div>
<div id="wrap"><img id="feed" alt="stream"></div>
<div id="tip">圆形 FOV 自瞄 · 实时绘制 · 双击画面全屏</div>
<script>
const img=document.getElementById('feed'),st=document.getElementById('st'),dot=document.getElementById('dot');
let n=0,t0=performance.now();
img.onload=()=>{n++;dot.className='dot ok';};
img.src='/stream.mjpg?'+Date.now();
setInterval(async()=>{try{const r=await fetch('/state.json',{cache:'no-store'});const j=await r.json();
  st.textContent='LIVE · lock #'+(j.lock_id??'-')+' · err '+(j.post_err??0).toFixed(2)+'px';
  document.getElementById('k').textContent='kernel: '+j.kernel_mode;
  document.getElementById('f').textContent='fps: '+j.push_fps;},catch(e){st.textContent='已断开';dot.className='dot';}},500);
document.body.addEventListener('dblclick',()=>{const e=document.documentElement;
  if(!document.fullscreenElement){e.requestFullscreen&&e.requestFullscreen();}else{document.exitFullscreen();}});
</script></body></html>
"""


class Hub:
    def __init__(self):
        self.cv = threading.Condition()
        self.jpeg: bytes | None = None
        self.seq = 0
        self.state: dict = {}
        self.clients = 0
        self._stamps: list[float] = []

    def push(self, jpeg: bytes, state: dict | None = None):
        import time

        with self.cv:
            self.jpeg = jpeg
            self.seq += 1
            if state:
                now = time.perf_counter()
                self._stamps.append(now)
                self._stamps = [t for t in self._stamps if now - t < 1.0]
                state = dict(state)
                state["push_fps"] = len(self._stamps)
                state["clients"] = self.clients
                self.state = state
            self.cv.notify_all()

    def wait(self, last_seq: int, timeout: float = 5.0):
        with self.cv:
            if self.seq == last_seq:
                self.cv.wait(timeout)
            return self.seq, self.jpeg


def make_handler(hub: Hub):
    class H(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        def do_GET(self):
            if self.path.startswith("/stream.mjpg"):
                return self._stream()
            if self.path.startswith("/state.json"):
                body = json.dumps(hub.state, ensure_ascii=False).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(body)
                return
            body = PAGE.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _stream(self):
            self.send_response(200)
            self.send_header("Age", "0")
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
            self.send_header("Pragma", "no-cache")
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()
            hub.clients += 1
            last = -1
            try:
                while True:
                    seq, jpeg = hub.wait(last, timeout=10.0)
                    if jpeg is None or seq == last:
                        continue
                    last = seq
                    self.wfile.write(b"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                     + str(len(jpeg)).encode() + b"\r\n\r\n" + jpeg + b"\r\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                hub.clients = max(0, hub.clients - 1)

    return H


def serve(hub: Hub, host: str = "0.0.0.0", port: int = 8788) -> ThreadingHTTPServer:
    srv = ThreadingHTTPServer((host, port), make_handler(hub))
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True).start()
    return srv


def encode_jpeg(img, quality: int = 78) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality, optimize=True)
    return buf.getvalue()
