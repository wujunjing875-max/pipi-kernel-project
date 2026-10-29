# 分支说明

## 当前分支布局

| 分支 | 内容 | 状态 | 说明 |
|---|---|---|---|
| **main** | `tpen_v33/TPEN_ESP_v33/` — v33-handover-2026-10-26 完整版 | ✅ 正式版 | clone 即可用，产物 md5 与 VERSION.txt 一致 |
| **HIS** | 仓库历史全量（abu-aim / js-aim / tpen_app / tpen_kernel / tpen-esp-portable / tpen_v40 / workspace_11-45 / branches / memory + 全部文档） | 📦 存档 | 只读追溯用，不再更新 |
| **master** | v40-radar-wall-2026-10-29（雷达 + 掩体开发线） | ⚠️ 开发中 | 功能未真机验证，产物与源码不匹配 |

## 历史子分支（保留在 HIS/master 树里）

以下分支内容早已合并，仅作记录：

- `feat/decrypt` — 解密功能线（已并入 v33）
- `feat/aimbot` — 自瞄线（运行时通过 `patch_aim.py` 注入，不在源码里）
- `feat/single-trans` — 单通道透传实验

## 为什么这样分

**问题的由来**：原仓库只有 `master` 一条线，里面 `tpen_v40/` 目录混着两代东西——
采集端源码是 v40 的（有 `cam=`/`vp=`/`w=` 雷达字段），
但仓库里原有的 `bin/p` 却是更早的产物（无这些字段），
导致「雷达和掩体在真机上根本不可能生效」这个问题一直查不出来。

同时 `master` 上还堆着 `abu-aim`（139MB）、`workspace_11-45`（45MB）、
`tpen_app`（23MB）这些和 v33 主线无关的素材，一个 clone 就是 418MB。

**于是拆成三条线**：

1. `main` 只放**能跑的正式版**（6.1MB），谁要部署就 clone 这条
2. `HIS` 把原来的全部内容原样封存，一个字节不改，随时能翻旧账
3. `master` 继续留作 v40 开发线，验证通了再往 main 合

## 常用操作

```bash
# 取正式版
git clone -b main https://gitee.com/nannan530/tpen-esp.git

# 翻历史素材（abu-aim、js-aim、APK 打包工程等）
git clone -b HIS https://gitee.com/nannan530/tpen-esp.git

# 看 v40 开发线
git clone -b master https://gitee.com/nannan530/tpen-esp.git

# 只拿某个分支的某个文件
git show HIS:tpen_app/build.sh
git show master:tpen_v40/TPEN_ESP_v40/src/C/paradise_fast100.c
```

## 版本纪律

每个版本必须交付 4 样东西，缺一不可：

1. `VERSION.txt` —— 真实 md5 + 字节数（**必须与产物实际 md5 一致**，历史上出过声明值是假的情况）
2. `CHANGELOG-vN.md` —— 本版改了什么
3. 产物必须**从本目录源码编译**得出，不能拿旧二进制凑数
4. `HANDOVER.md` 同步更新

详见 `VERSIONING.md`（在 master 与 HIS 分支上）。
