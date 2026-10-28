# UZI ESP — 可移植工作包

> 暗区突围（`com.tencent.mf.uam`）外部 ESP 的完整工程包。
> 包含源码、编译产物、驱动、资源、工具链说明，可以在另一台电脑上直接接着干。

---

## 这个包是什么

一套**内核读取 + 外部绘制**的 ESP，分三层：

| 层 | 语言 | 作用 | 运行位置 |
|---|---|---|---|
| **采集端** | C（NDK 静态编译） | 通过内核驱动读游戏内存，算出屏幕坐标，写 `/data/local/tmp/esp.txt` | 设备 root shell |
| **绘制端** | Java（app_process 跑 dex） | 读 esp.txt，用 SurfaceFlinger 直接画 overlay | 设备 root shell |
| **外壳 APK** | Java（Android） | 解压资源、刷驱动、启动/停止上面两个进程 | 普通 App |

采集端和绘制端**完全解耦**，靠一个文本文件通信，所以可以单独替换任意一端。

---

## 目录结构

```
uzi-esp-portable/
├── README.md                  ← 本文件
├── docs/
│   ├── 01-架构说明.md          三层架构、数据流、为什么这么设计
│   ├── 02-驱动对接.md          驱动 magic、内核模块、insmod、多版本适配
│   ├── 03-偏移表.md            ★ 所有已知偏移（含可信度和验证方法）
│   ├── 04-编译与部署.md        NDK 编译命令、dex 编译、设备部署步骤
│   ├── 05-踩坑记录.md          ★ 踩过的所有坑，别重踩
│   └── 06-待解决问题.md        加密、头甲、全图人数等已知缺陷
├── src/
│   ├── C/paradise_fast100.c   采集端源码（~1000 行）
│   ├── Java/Ov50.java         绘制端源码（~2000 行）
│   └── apk/                   Main.java + pack.sh
├── bin/paradise_fast100       采集端已编译产物（aarch64，直接可跑）
├── dex/classes.dex            绘制端已编译产物
├── apk/app-release.apk        成品 APK（5.8MB，含 6 个内核驱动）
├── tools/
│   ├── uzi_dump.c / uzi_dump  内存快照工具（调研用）
│   ├── diff_snapshot.py       两份内存快照对比
│   ├── diff_files.py          两份文件快照对比
│   └── start_esp.sh           一键启动脚本
├── assets/                    绘制资源（字形图集、各类表）
├── drivers/drv_*.sh           6 个内核版本的自包含驱动脚本
└── versions/                  历史版本归档（可选保留）
```

---

## 快速开始（3 步）

### 1. 部署到设备

```bash
# 前提：设备已 root，adb 可用
adb push bin/paradise_fast100        /data/local/tmp/p
adb push dex/classes.dex             /data/local/tmp/o.dex
adb push drivers/drv_61.sh           /data/local/tmp/drv.sh   # 按内核版本选，见 02-驱动对接.md
adb push assets/g.png                /data/local/tmp/glyph.png
adb push assets/g.txt                /data/local/tmp/glyph.txt
adb push assets/ui.png               /data/local/tmp/ov_ui.png
adb push assets/ui.txt               /data/local/tmp/ov_ui.txt
adb push assets/ctype.txt            /data/local/tmp/ov_ctype.txt
adb push assets/level.txt            /data/local/tmp/ov_level.txt
adb push assets/name.txt             /data/local/tmp/ov_name.txt
adb push assets/price.txt            /data/local/tmp/ov_price.txt
adb push assets/skip.txt             /data/local/tmp/ov_skip.txt
adb push tools/start_esp.sh          /data/local/tmp/start_esp.sh

# 整理权限
adb shell 'su -c "mkdir -p /data/local/tmp/.c; cp /data/local/tmp/p /data/local/tmp/.c/p; cp /data/local/tmp/o.dex /data/local/tmp/.c/o.dex; cp /data/local/tmp/drv.sh /data/local/tmp/.c/drv.sh; chmod 755 /data/local/tmp/.c/p /data/local/tmp/.c/drv.sh /data/local/tmp/start_esp.sh; chmod 644 /data/local/tmp/.c/o.dex /data/local/tmp/glyph.* /data/local/tmp/ov_*"'
```

### 2. 启动

```bash
adb shell 'su -c "sh /data/local/tmp/start_esp.sh"'
```

脚本会做：加载驱动 → 起绘制端 → 起采集端，并打印 `esp.txt` 头部确认。

### 3. 验证

```bash
adb shell 'su -c "grep -E \"^pl=|^enz=|^ace=\" /data/local/tmp/esp.txt; grep \"frame \" /data/local/tmp/.c/o.log | tail -1"'
```

正常会看到类似：

```
pl=3            ← 260m 内真人数
enz=2669 aiz=2255 zspan=808 dz=414 enN=2 aiN=7 enc4=0
ace=0 -         ← ACE 目录无写入
[*] frame 1800 fps=50 [...]   ← 绘制端在出帧
```

---

## 用 APK 的方式（不想手动部署时）

直接装 `apk/app-release.apk`，界面里：
- **刷驱动**：下拉选内核版本 → 「刷入选中」，或点「智能刷入」自动匹配
- **启动 ESP / 退出 ESP**：一键起停，内部就是上面那些 adb 命令在做的事

---

## 当前能用的 / 不能用的

| 功能 | 状态 | 说明 |
|---|---|---|
| 真人方框 + 名字 | ✅ 可用 | 名字来自 `PlayerState+0x440`，实机验证过 |
| 人机 / 人机玩家 | ✅ 可用 | 青色框 |
| 头甲等级 | ⚠️ **仅头盔准确** | `actor+0x1f80`=头盔；**护甲偏移尚未找到**，见 06 |
| 物资 / 容器 | ✅ 可用 | 类别表 + 价格表可改 |
| 骨骼 | ✅ 可用 | 28 根骨骼连线 |
| 全图剩余人数 | ❌ 不准 | `GameState.PlayerArray` 偏移未定位，当前显示的是 260m 内计数 |
| 坐标加密检测 | ⚠️ 部分 | 真人对比通道准（需真人在场）；ACE 文件通道待验证 |
| 加密态画真人 | ❌ 画不出来 | 加密时坐标是假的，尚未实现解密 |

**详细缺陷分析见 `docs/06-待解决问题.md`。**

---

## 关键数据速查

| 项目 | 值 |
|---|---|
| 游戏包名 | `com.tencent.mf.uam` |
| 引擎 | UE4（`libUE4.so`） |
| 驱动 type | `__NR_reboot` syscall hook（LKM） |
| 驱动 magic | `0x7a3b9c4d` / `0x2f8e1d6b` / cmd `0x1234567` |
| 通信文件 | `/data/local/tmp/esp.txt`（采集端写、绘制端读） |
| 自标定 | `ctrl_off=0x348`、`vest_off`（运行时学习） |
| NDK | r26d（aarch64-linux-android21） |

---

## 搬运到新电脑时注意

1. **NDK 路径**写死在多处脚本里：`D:/WRAI/REA/.tools/ndk/android-ndk-r26d`，换机器要改。
2. **JDK 路径**同样（`05_tools/build_ov.sh` 里）。
3. `pack.sh` 里的 `KEY`/`IV` 是 AES 资源加密用的，换机器保留原值即可。
4. 详见 `docs/04-编译与部署.md`。

---

## 安全提示

- 这份工程用于**安全研究 / 逆向学习**，请勿用于破坏游戏公平性。
- 驱动以 root 权限 hook 内核 syscall，本身有稳定性风险，测试机建议独立设备。
- 加密触发条件与账号行为相关（被举报、设备异常），实测会连带影响账号状态。
