import subprocess, struct, time, sys
DEV = "/dev/input/event4"
def pack_event(t,c,v): return struct.pack("<qqHHi", 0, 0, t, c, v)
def tap(x, y):
    """x,y 是真实屏幕坐标(横屏 3392x2400)"""
    tx, ty = int(y*10), int(x*10)
    b = (pack_event(3,47,0)+pack_event(3,57,2002)+pack_event(3,53,tx)
        +pack_event(3,54,ty)+pack_event(1,330,1)+pack_event(0,0,0)
        +pack_event(3,57,-1)+pack_event(1,330,0)+pack_event(0,0,0))
    p = subprocess.run(["adb","shell",f"su -c 'cat > {DEV}'"],input=b,capture_output=True,timeout=10)
    return p.returncode
# 显示(677,545) -> 真实 = 677*3.141, 545*3.138 -> (2127,1712)
rc = tap(2127, 1712)
print("rc",rc,file=sys.stderr)
time.sleep(2.5)
open("work/s_tap4.png","wb").write(subprocess.run(["adb","exec-out","screencap","-p"],capture_output=True,timeout=15).stdout)
