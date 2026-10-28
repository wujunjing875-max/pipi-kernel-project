# -*- coding: utf-8 -*-
"""二进制直写 /dev/input/event4 验证 —— input_event 24B (aarch64)
点开桌面上已知坐标的图标，验证高频注入通路"""
import subprocess, struct, time, sys

DEV = "/dev/input/event4"
SW_W, SW_H = 2400, 3392   # 竖屏物理
TP_MAX_X, TP_MAX_Y = 24000, 33920

def pack_event(t, c, v, sec=0, usec=0):
    return struct.pack("<qqHHi", sec, usec, t, c, v)

def tap_bytes(x, y, touch_x_is_display_y=True):
    """display(x,y)横屏 -> touch(X=display_y*10, Y=display_x*10)"""
    tx = int(y * 10); ty = int(x * 10)
    buf = b""
    buf += pack_event(3, 47, 0)            # ABS_MT_SLOT 0
    buf += pack_event(3, 57, 2001)         # ABS_MT_TRACKING_ID
    buf += pack_event(3, 53, tx)           # ABS_MT_POSITION_X
    buf += pack_event(3, 54, ty)           # ABS_MT_POSITION_Y
    buf += pack_event(1, 330, 1)           # BTN_TOUCH down
    buf += pack_event(0, 0, 0)             # SYN
    buf += pack_event(3, 57, -1)           # release
    buf += pack_event(1, 330, 0)           # BTN_TOUCH up
    buf += pack_event(0, 0, 0)             # SYN
    return buf

def push(buf):
    p = subprocess.run(["adb", "shell", f"su -c 'cat > {DEV}'"],
                       input=buf, capture_output=True, timeout=10)
    return p.returncode

if __name__ == "__main__":
    subprocess.run(["adb", "shell", "input keyevent 3"])
    time.sleep(0.8)
    # 底部 dock 第一个图标（显示约 340,708 -> 屏幕(1068,2221)横屏坐标）
    dx, dy = 1068.0, 2221.0
    rc = push(tap_bytes(dx, dy))
    print("push rc =", rc, file=sys.stderr)
    time.sleep(2.5)
    with open("work/s_binary_tap.png", "wb") as f:
        f.write(subprocess.run(["adb", "exec-out", "screencap", "-p"],
                 capture_output=True, timeout=15).stdout)
    print("screenshot saved", file=sys.stderr)
