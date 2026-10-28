"""目标采集后端。

同一套引擎可以吃三种数据源，切换只改一行：
  TcpBackend      —— 本机 Demo 靶场（进程外，走 TCP，模拟"跨进程读实体"的时序与延迟）
  ProcessBackend  —— 真实游戏，RPM / 内核 IOCTL 读实体链表（填 offsets.json 即可用）
"""
from __future__ import annotations

import json
import socket
import struct
import time


class TcpBackend:
    name = "tcp://demo-arena"

    def __init__(self, host="127.0.0.1", port=27655, timeout=5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.f = self.sock.makefile("rwb")

    def _call(self, payload: dict) -> dict:
        self.f.write((json.dumps(payload) + "\n").encode())
        self.f.flush()
        line = self.f.readline()
        if not line:
            raise ConnectionError("server closed")
        return json.loads(line)

    def handshake(self) -> dict:
        return self._call({"cmd": "ping"})

    def step(self, n: int) -> dict:
        snap = self._call({"cmd": "step", "n": n})
        snap["_t_recv"] = time.perf_counter()
        return snap

    def snapshot(self) -> dict:
        return self._call({"cmd": "state"})

    def close(self):
        try:
            self.f.close()
            self.sock.close()
        except Exception:
            pass


class ProcessBackend:
    """外部 ESP 读取器。offsets.json 结构见文件内注释，换游戏只改偏移。"""

    name = "rpm://process"

    FIELDS = ["x", "y", "vx", "vy", "hp", "team", "alive"]

    def __init__(self, client, pid: int, offsets_path: str):
        with open(offsets_path, "r", encoding="utf-8") as fp:
            self.off = json.load(fp)
        self.client = client
        self.client.attach(pid)
        self.module_base = self._module_base(pid)

    def _module_base(self, pid: int) -> int:
        import ctypes
        import ctypes.wintypes as wt
        from ctypes import wintypes

        TH32CS_SNAPMODULE = 0x8

        class MODULEENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("th32ModuleID", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("GlblcntUsage", wintypes.DWORD),
                ("ProccntUsage", wintypes.DWORD),
                ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
                ("modBaseSize", wintypes.DWORD),
                ("hModule", wt.HMODULE),
                ("szModule", ctypes.c_char * 256),
                ("szExePath", ctypes.c_char * 260),
            ]

        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
        me = MODULEENTRY32()
        me.dwSize = ctypes.sizeof(MODULEENTRY32)
        want = self.off.get("module", "").lower()
        if k32.Module32First(snap, ctypes.byref(me)):
            while True:
                if me.szModule.decode(errors="ignore").lower() == want:
                    base = ctypes.cast(me.modBaseAddr, ctypes.c_void_p).value
                    k32.CloseHandle(snap)
                    return base
                if not k32.Module32Next(snap, ctypes.byref(me)):
                    break
        k32.CloseHandle(snap)
        raise RuntimeError(f"module {self.off.get('module')} not found in pid {pid}")

    def _u64(self, addr: int) -> int:
        return struct.unpack("<Q", self.client.read(addr, 8))[0]

    def _f32(self, addr: int) -> float:
        return struct.unpack("<f", self.client.read(addr, 4))[0]

    def snapshot(self) -> dict:
        o = self.off
        root = self.module_base + o["root_rva"]
        ent = self._u64(root)                      # 实体管理器
        arr = self._u64(ent + o["list_ptr_rva"])
        cnt = struct.unpack("<i", self.client.read(ent + o["count_rva"], 4))[0]
        stride = o["stride"]
        out = []
        for i in range(min(cnt, 64)):
            base = arr + i * stride
            t = {"id": i}
            for name in self.FIELDS:
                rva = o["fields"].get(name)
                if rva is None:
                    continue
                if name in ("team", "alive"):
                    t[name] = struct.unpack("<i", self.client.read(base + rva, 4))[0]
                else:
                    t[name] = self._f32(base + rva)
            if not t.get("alive", 1):
                continue
            out.append(t)
        return {"tick": -1, "targets": out}

    def step(self, n: int) -> dict:
        return self.snapshot()

    def close(self):
        pass
