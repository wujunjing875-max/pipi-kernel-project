# 分支约定（TPEN ESP 双人协作）

> ## ⚠️ 2026-10-29 分支重构，先看这里
>
> 仓库已拆成三条线，**默认分支已切为 `main`**：
>
> | 分支 | 内容 | 状态 |
> |---|---|---|
> | **`main`**（默认） | v33-handover-2026-10-26 完整版，`tpen_v33/TPEN_ESP_v33/`，6.2MB / 66 文件 | ✅ 正式生产版，clone 即可用 |
> | **`HIS`** | master 全量快照（293 文件，含 abu-aim 139M / workspace 45M / tpen_app 23M 等全部素材） | 📦 只读存档，一个字节不改 |
> | **`master`**（本分支） | v40-radar-wall 开发线 | ⚠️ 雷达/掩体未真机验证 |
>
> **为什么要拆**：原来 master 上 `tpen_v40/` 的采集端源码是 v40 的（有 `cam=`/`vp=`/`w=`），
> 但仓库里原有的 `bin/p` 是更早的产物（无这些字段），导致「雷达和掩体真机不生效」一直查不出来。
> 现在 `main` 放确定能跑的 v33，`HIS` 封存全部历史，`master` 继续开发。
>
> 下面的「双人协作分支约定」写的是 master 开发线内部的规矩，仍然有效。

主线只有一个：`master`。三条功能分支按方向切，谁做什么就待在谁的分支里，**不要直接往 master 提**。

## 分支表

| 分支 | 方向 | 目录归属 | 说明 |
|---|---|---|---|
| `master` | 主线 / 集成 | — | 只接受三个功能分支的 merge，不直接开发 |
| `feat/single-trans` | 单透（ESP 透视） | `branches/single-trans/` | 采集端 → esp.txt → overlay 渲染链路 |
| `feat/decrypt` | 解密（坐标解密开关） | `branches/decrypt/` | `ov_cfg` 的 dec/dec2/dec3/dec4 四档 |
| `feat/aimbot` | 自瞄 | `branches/aimbot/` | abu-aim 像素瞄准 / js-aim 驱动侧 |

## 规矩

1. **目录隔离**：每条分支只改自己 `branches/<方向>/` 下的文件。碰别人目录一定冲突。
2. **公共代码走 master**：要改 `tpen_app/`、`tpen_kernel/`、`tpen-esp-portable/`、`tpen_v40/` 这种公共内核，先在群里说一声，改完走 PR 进 master，再由其它分支 `git merge master` 同步。
3. **对齐节奏**：每完成一个可验证节点，就 `git merge master` 拉一次，然后提 PR 回 master。两边 master 必须一致。
4. **不推二进制垃圾**：调试截图、日志、临时 dump 一律不进仓库（`.gitignore` 已挡）。

## 日常命令

```bash
# 拉主线最新
git checkout master && git pull

# 回到自己的方向分支并同步主线
git checkout feat/single-trans
git merge master

# 干完活提交
git add -A && git commit -m "single-trans: 修 xxx"
git push

# 对齐：到网页提 PR，对方 review 后合进 master
```

## 首次 clone

```bash
git clone git@gitee.com:nannan530/tpen-esp.git
cd tpen-esp
git branch -a          # 看得到全部四条
```

> 仓库是私有的，克隆前要把自己的 SSH 公钥挂到 Gitee 账号（设置 → SSH 公钥），否则会 `Permission denied (publickey)`。
