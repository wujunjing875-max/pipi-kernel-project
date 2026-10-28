# JianShangAim.sys 构建与加载

## 构建

1. VS2022 + WDK 11，新建 **Empty WDM Driver (x64)** 工程
2. 加入 `JianShangAim.c`
3. Release | x64 构建 → 产物拷到 `js-aim/driver/JianShangAim.sys`

## 签名（测试机）

```
bcdedit /set testsigning on     # 重启后生效
bcdedit /set nointegritychecks on
```

正式分发需要 EV 证书签 + 微软硬件门户 attestation 签名，这里按测试机标准走。

## 加载（管理员）

```
sc create JianShangAim type= kernel start= demand binPath= C:\path\to\JianShangAim.sys
sc start JianShangAim
sc query JianShangAim
```

## 验证

```
python run_test.py
# 期望输出：
# [kernel] mode=kernel  win32_error=0  驱动设备已打开，IOCTL 通路可用
```

## IOCTL 接口

| CTL | 功能 | 输入 | 输出 |
|---|---|---|---|
| 0x801 (IOCTL_JS_READ) | 读目标进程内存 | `JS_REQ{Pid, Address, Size}` | `Data[Size]` |
| 0x802 (IOCTL_JS_WRITE) | 写目标进程内存 | `JS_REQ{Pid, Address, Size, Data[Size]}` | — |

IOCTL 通路绕过：
- OpenProcess 的 PROCESS_VM_READ 权限要求
- PPL 进程保护
- ObRegisterCallbacks 剥句柄（不走句柄，直接 MmCopyVirtualMemory）

## 未编译驱动时的行为

`core/kernel.py::probe()` 探测失败 → `KernelClient` 自动降级 user 模式
（OpenProcess + ReadProcessMemory），引擎与自瞄逻辑完全不变，
报告里如实标注 `kernel_mode=user` 与失败原因。
