# TPEN ESP — 双人协作仓库

TPEN ESP 内核启动器的全套源码与开发记忆。私有仓库，两个人在这个上面并行开发、对齐进度。

## 先看哪份

| 文件 | 内容 |
|---|---|
| [`BRANCHES.md`](BRANCHES.md) | **分支怎么用（必读）**，四条分支各管什么、怎么合 |
| [`PROGRESS.md`](PROGRESS.md) | **进度对齐表**，谁做到哪一步、卡在哪 |
| [`交接说明.md`](交接说明.md) | 项目全貌、每个工程是什么、当前状态 |
| `memory/` | AI 助手的开发记忆，按日期记的决策和坑，接手必读 |

## 四条分支

```
master              主线，只收 merge，不直接开发
├── feat/single-trans   单透（只画玩家一类）
├── feat/decrypt        解密（dec/dec2/dec3/dec4 四档）
└── feat/aimbot         自瞄（abu-aim 像素 / js-aim 驱动）
```

每条分支的技术说明在 `branches/<方向>/README.md`，待办在 `TODO.md`。

## 工程目录

| 目录 | 说明 |
|---|---|
| `tpen_app/` | **主战场**：内核启动器 APK 工程（com.tpen.esp）。`assets/p` 内核二进制、`assets/ov.dex` 悬浮窗、`out/TPEN.apk` 最新构建（剔除自瞄版） |
| `tpen_kernel/` | 内核工程副本 |
| `tpen_v33/TPEN_ESP_v33/` | v33 工程：`src/Java/Ov50.java` 悬浮窗主逻辑、`scripts/` 部署脚本、`CHANGELOG-ALL.md` 完整变更史 |
| `tpen-esp-portable/` | 可移植版，`versions/` 有 v4.5.0~v4.7.0 快照，`snapshots/` 是 enc_on / noenc_hot 两种现场抓包 |
| `abu-aim/` | 自瞄实验：`AimProvider`/`PixelAimProvider`，`patch_wgn_kernel.py` |
| `js-aim/` | 自瞄实验二：Windows 内核驱动 `JianShangAim.sys` + 用户态 |
| `workspace_11-45/` | 主工作区杂物：`ov_cfg.txt` 配置、`setcfg.sh`、雷达屏蔽片段 |
| `memory/` | 开发记忆 |

## 克隆

```bash
git clone git@gitee.com:nannan530/tpen-esp.git
cd tpen-esp
git branch -a
```

走 HTTPS 就用 `https://gitee.com/nannan530/tpen-esp.git`，密码填**私人令牌**，不是账号密码。

克隆前先把自己的 SSH 公钥挂到 Gitee（设置 → SSH 公钥），否则 `Permission denied (publickey)`。

## 构建

```bash
cd tpen_app && python build_apk.py     # 产出 out/TPEN.apk
```

## 设备部署（平板）

```bash
adb push tpen_app/assets/p /data/local/tmp/.c/p
adb push tpen_app/assets/ov.dex /data/local/tmp/.c/o.dex
adb push workspace_11-45/ov_cfg.txt /data/local/tmp/ov_cfg.txt
adb shell su -c 'sh /data/local/tmp/.c/start_esp.sh'
```

## 三条铁律

1. **master 不直接开发**，只收 merge。
2. **只改自己分支的 `branches/<方向>/`**，要动公共代码先在群里说。
3. **改完一个节点就更新 `PROGRESS.md`**，这样两边进度永远对得上。

QQ群 301930102
