# 自瞄分支（feat/aimbot）

两个独立实现，别混：

| 工程 | 平台 | 目标来源 | 状态 |
|---|---|---|---|
| `abu-aim/` | Android（平板） | root 截屏 + 像素找色 | 第一版跑通，主线已剔除 |
| `js-aim/` | Windows | 内核驱动读内存 | 实验工程，仅参考 |

主线（`master` 的 `out/TPEN.apk`）是**剔除自瞄版**。要加自瞄就待在这条分支，别把自瞄合进主线。

## abu-aim：接口 + 像素实现

```java
// abu-aim/app/java/com/jianshang/aim/AimProvider.java
public interface AimProvider {
    /** 返回目标屏幕坐标 [x,y]（横屏 3392x2400 坐标系），无目标返回 null */
    float[] find(Context ctx, float cx, float cy, float radius);
}
```

设计意图写在注释里：**换成 UE4 内存读取时只替换实现即可**。

第一版实现 `PixelAimProvider`：

1. `shoot()` → root 截屏到 `/data/local/tmp/js_aim.png`
2. 解码 → 宽降采样到 ≤360（省 CPU）
3. 在自瞄圈内找"红色能量重心"（血条 / 人物高亮）当目标点
4. 45ms 限速（`lastShot`），避免截图进程堆积

换成内存读坐标，只要新写一个 `AimProvider` 实现：

```java
public class MemAimProvider implements AimProvider {
    @Override public float[] find(Context ctx, float cx, float cy, float radius) {
        // UWorld -> Actors -> Head 屏幕投影，返回 {x,y} 或 null
        return null;
    }
}
```

## js-aim：驱动通道

```
JianShangAim.sys  ← 内核驱动，暴露 \\.\JianShangAim
   ↓ IOCTL 0x801 读 / 0x802 写
MmCopyVirtualMemory  ← 绕 PPL / ObRegisterCallbacks / 句柄权限限制
   ↓
core/kernel.py probe() → mode=kernel
```

构建（WDK + VS，x64 Free）：

1. 新建 "Empty WDM Driver" 工程，加入 `driver/JianShangAim.c`
2. 目标平台 x64，配置 Release
3. 测试签名：`bcdedit /set testsigning on` 后重启
4. 产物 `JianShangAim.sys` 放回 `driver/`

加载：

```cmd
sc create JianShangAim type= kernel start= demand binPath= C:\path\JianShangAim.sys
sc start JianShangAim
```

请求结构（IOCTL 里用的）：

```c
typedef struct _JS_REQ {
    ULONG  Pid;
    PVOID  Address;
    ULONG  Size;
    UCHAR  Data[1];
} JS_REQ, *PJS_REQ;
```

## v33 里的自瞄补丁（历史）

- `tpen_v33/TPEN_ESP_v33/patch_aim.py` / `patch_aim2.py` —— 给 Ov50 打自瞄补丁的脚本
- `tpen_v33/TPEN_ESP_v33/src/Java/Ov50.java.bak_aim` —— **带自瞄的旧备份**，当前主线版本已剔除
- `tpen_v33/TPEN_ESP_v33/out/aim.jar` —— 当时的构建产物

要看旧自瞄逻辑，对比 `Ov50.java.bak_aim` 和当前的 `Ov50.java` 就是全部差异。

## 已知坑

- 像素找色依赖血条颜色，**换皮肤 / 换画质 / 开 HDR 都会漏检**，这是第一版的固有限制。
- 截屏走 root，频率高了会拖帧；45ms 是被验证过的下限。
- 内核驱动必须测试签名，正式签名驱动加载会被拒。
- `3392x2400` 是横屏坐标基准，跟 `ov_cfg` 里 `aff` 矩阵的分辨率是两套，别串。
