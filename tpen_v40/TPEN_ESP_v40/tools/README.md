# tools/ 逆向分析工具

全部是独立的小 C 程序，用 NDK 交叉编译后推到设备跑。
统一套路：`clang --target=aarch64-linux-android21 -O2 -fPIE -pie --sysroot=<ndk_sysroot> -o <bin> <src> -lm`

| 工具 | 作用 |
|---|---|
| `cmp_pos.c` | 对比 actor 的 root+0x154 与 mesh+0x8C0 两路坐标是否同源 |
| `mat_chk.c` | 校验投影矩阵链（`G_ENGINE → +0x20 → +0x2A0`）：正交性 + 活体投影 |
| `dump_chars.c` | 打印所有 Character 的 ua / t154 / t614 / 距离 / 控制器类名 |
| `find_ua.c` | 自标定分类枚举偏移（自己必须=3，取值只允许 1/2/3） |
| `find_type.c` | 差分扫描"能区分特遣/普通人机"的字段（注意：0x614 不是特遣专属） |
| `find_proxy.c` | 列出所有 Proxy/Rep/Scene 类对象及其坐标 |
| `proxy_verify.c` | 代理体 owner ↔ 角色 root 坐标对照（证明加密） |
| `proxy_track.c` | 代理坐标实时性验证（看它是否跟着人动） |
| `proxy_rate.c` | 量化代理坐标的更新频率 |
| `find_fast.c` | 扫 FastRepProxy / SGCharacterProxy / ownerActor 找高频坐标 |
| `find_fast2.c` | 遍历加密角色所有指针字段，找"会动的坐标三元组" |
| `corr.c` | 常驻相关性扫描（候选字段 vs 代理真值） |
| `corr2.c` | 扩展到子对象（40 个组件）的坐标候选扫描 |
| `vscan.c` | **按值搜索**：拿代理真坐标当特征，全内存找副本（注意覆盖盲区，见 HANDOVER 9.3） |
| `vscan6.c` | 邻域扫描（proxy/actor ±N MB），带覆盖自检 |
| `findcopy.c` | 一次跑完：扫描 ±64MB → 立即对命中点采样对比更新次数 |
| `dupcmp.c` | 对同一坐标的所有副本做更新频率对比 |
| `chcmp.c` | **三通道（0x314/0x330/0x11050）刷新率 + 同值比例实测** |
| `enc_probe.c` | 加密态现场取证：原始坐标 / 链试探 / z 统计 |
| `enc_catch.c` | 后台守候：出现非自己的玩家类对象立刻 dump 0x2400 原始内存 |
| `enc_deep.c` | 深挖加密角色：root/mesh/PlayerState/ArmorMgr 多对象坐标扫描 |
| `probe88.c` | 单字段跟踪探针（模板，改偏移即可复用） |

## 典型用法

```bash
# 三通道刷新率（回答"哪路最快"）
adb shell "su -c 'cd /data/local/tmp/.c && ./chcmp'"

# 找更新更快的副本（一次跑完，最省事）
adb shell "su -c 'cd /data/local/tmp/.c && ./findcopy'"

# 加密态取证
adb shell "su -c 'cd /data/local/tmp/.c && ./enc_probe'"
```

## 编译示例

```bash
NDK=/path/to/android-ndk-r26d
$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe \
  --target=aarch64-linux-android21 -O2 -fPIE -pie \
  --sysroot=$NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot \
  -o chcmp tools/chcmp.c -lm
adb push chcmp /data/local/tmp/.c/chcmp
```
