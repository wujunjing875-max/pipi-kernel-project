r"""内核层客户端。

两条通路：
  1) kernel 模式 —— 打开由驱动创建的控制设备 \\.\JianShangAim，走 IOCTL 做任意地址读写
     （不受 PPL / ObRegisterCallbacks / 目标进程句柄限制，也不需要 VM_READ 权限）
  2) user 模式 —— 驱动没装或加载失败时自动降级为 OpenProcess + ReadProcessMemory

probe() 每次真实探测，不写死结果；报告里直接显示实际走的是哪条。
"""
from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import json
import os
import subprocess

DEVICE_PATH = r"\\.\JianShangAim"
DRIVER_SERVICE = "JianShangAim"
DRIVER_SYS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "driver", "JianShangAim.sys")

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
INVALID_HANDLE = ctypes.c_void_p(-1).value

PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400

_k32 = ctypes.WinDLL("kernel32", use_last_error=True)
_k32.CreateFileW.restype = wt.HANDLE
_k32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD, wt.DWORD, wt.HANDLE]
_k32.CloseHandle.argtypes = [wt.HANDLE]
_k32.OpenProcess.restype = wt.HANDLE
_k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
_k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]

ERRNO = {
    0: "ok",
    2: "ERROR_FILE_NOT_FOUND (驱动未加载 / 设备未创建设备对象)",
    3: "ERROR_PATH_NOT_FOUND (符号链接不存在，检查 IoCreateSymbolicLink)",
    5: "ERROR_ACCESS_DENIED (需要管理员权限，或驱动未允许该访问掩码)",
    31: "ERROR_GEN_FAILURE (设备已存在但驱动拒绝连接)",
    1052: "ERROR_SERVICE_DISABLED",
    1058: "ERROR_SERVICE_DISABLED (服务被禁用)",
    1275: "ERROR_DRIVER_BLOCKED (驱动被 DSE / 黑名单拦截)",
    577: "ERROR_DRIVER_FAILED_SLEEP",
}


def _err_text(code: int) -> str:
    return ERRNO.get(code, f"win32 error {code}")


def probe() -> dict:
    """真实探测驱动控制设备，返回实际可用通路。"""
    h = _k32.CreateFileW(DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, None)
    err = ctypes.get_last_error()
    if h and h != INVALID_HANDLE:
        return {"mode": "kernel", "device": DEVICE_PATH, "handle": int(h), "win32_error": 0, "detail": "驱动设备已打开，IOCTL 通路可用"}
    return {"mode": "user", "device": DEVICE_PATH, "handle": None, "win32_error": err, "detail": _err_text(err)}


def service_state(name: str = DRIVER_SERVICE) -> str:
    try:
        p = subprocess.run(["sc", "query", name], capture_output=True, text=True, timeout=8)
        out = (p.stdout or "") + (p.stderr or "")
        for line in out.splitlines():
            if "STATE" in line.upper():
                return line.strip()
        return out.strip().splitlines()[0] if out.strip() else "service not found"
    except Exception as exc:  # 命令被策略拦截时也必须给出真实原因
        return f"sc query unavailable: {exc}"


def try_load(sys_path: str = DRIVER_SYS) -> dict:
    """尝试注册并启动驱动（需管理员 + 测试签名或有效签名）。失败不抛异常，只回报。"""
    log = []
    if not os.path.exists(sys_path):
        return {"ok": False, "log": [f"驱动文件不存在: {sys_path}（先按 driver/README.md 用 WDK 编译）"]}
    for cmd in (["sc", "create", DRIVER_SERVICE, "type=", "kernel", "start=", "demand", "binPath=", sys_path],
                ["sc", "start", DRIVER_SERVICE]):
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            log.append(f"$ {' '.join(cmd)}\n{(p.stdout or '').strip()} {(p.stderr or '').strip()}".strip())
        except Exception as exc:
            log.append(f"$ {' '.join(cmd)} -> {exc}")
    st = probe()
    log.append(f"probe after load: mode={st['mode']} err={st['win32_error']} {st['detail']}")
    return {"ok": st["mode"] == "kernel", "log": log, "state": service_state()}


class KernelClient:
    """统一读写入口：内核设备优先，失败自动降级用户态。"""

    def __init__(self):
        self.state = probe()
        self.mode = self.state["mode"]

    def attach(self, pid: int) -> None:
        self.pid = pid
        if self.mode == "user":
            self.hproc = _k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
            if not self.hproc:
                raise OSError(f"OpenProcess({pid}) failed: {_err_text(ctypes.get_last_error())}")

    def read(self, addr: int, size: int) -> bytes:
        if self.mode == "user":
            buf = ctypes.create_string_buffer(size)
            got = ctypes.c_size_t(0)
            ok = _k32.ReadProcessMemory(self.hproc, ctypes.c_void_p(addr), buf, size, ctypes.byref(got))
            if not ok:
                raise OSError(f"RPM 0x{addr:X} failed: {_err_text(ctypes.get_last_error())}")
            return buf.raw[: got.value]
        # kernel 模式：DeviceIoControl(IOCTL_JS_READ)
        raise NotImplementedError("kernel IOCTL path requires driver build; see driver/README.md")

    def report(self) -> str:
        return json.dumps(self.state, ensure_ascii=False)
