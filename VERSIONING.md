# 版本迭代规范（TPEN ESP）

> 本文件是版本号的唯一权威说明。改版本号、加版本目录，先看这里。

## 1. 版本号格式

```
vN-<主题>-<YYYY-MM-DD>
```

例：`v40-radar-wall-2026-10-29`

- `N`：纯数字，**单调递增，不重复、不回退**
- `<主题>`：英文小写 + 短横线，概括这一版最核心的改动（`decrypt` / `radar-wall` / `fix-bone`）
- `<日期>`：打包日期

## 2. 目录规则

```
仓库根/
└── tpen_vN/
    └── TPEN_ESP_vN/
        ├── VERSION.txt        本版产物 md5（必须与真实文件一致）
        ├── CHANGELOG-vN.md    本版改了什么（相对上一版）
        ├── CHANGELOG-ALL.md   历史累积变更（沿用旧文件，不重写）
        ├── HANDOVER.md        交接文档（当前有效）
        ├── src/C/             采集端源码
        ├── src/Java/          绘制端源码
        ├── bin/p              采集端产物（编译后放这里）
        ├── dex/classes.dex    绘制端产物
        ├── assets/ drivers/ scripts/ tools/ docs/
        └── legacy/            本版废弃的产物（附 README 说明为什么废）
```

**每一版都是完整可编译的一套**，不依赖上一版目录。
旧版本目录可保留做历史，但**只保留源码与文档，产物不进仓库**（`.gitignore` 已挡）。

## 3. 每一版必须交齐的四件

| # | 项 | 要求 |
|---|---|---|
| 1 | `VERSION.txt` | **真实 md5 + 真实字节数**，编译完再填。写错了等于没有 |
| 2 | `CHANGELOG-vN.md` | 相对上一版的改动、协议变更、发现的问题、遗留待办 |
| 3 | 产物 | `bin/p` 与 `dex/classes.dex` 必须是**用本目录源码编出来的** |
| 4 | `HANDOVER.md` | 版本号、目录结构、新增模块同步更新 |

## 4. 加版本号之前必须做的自检

```bash
# 1) 产物是不是用这份源码编的？（最常见的坑）
python -c "d=open('bin/p','rb').read(); print('cam=%',b'cam=%' in d,'w=%d',b'w=%d' in d)"
# 源码里有 cam=/w= 而产物里没有 → 产物是旧的，重编

# 2) dex 里有没有本版新方法？
python -c "d=open('dex/classes.dex','rb').read(); print('drawRadar',b'drawRadar' in d)"

# 3) md5 与实际文件是否一致？
md5sum bin/p dex/classes.dex
cat VERSION.txt    # 逐字比对
```

**v33 就是栽在第 1 条上**：源码有 `cam=`/`w=`/`s=<i>`，但 `bin/p` 里一个都没有，
导致雷达和掩体在真机上前提就不成立。

## 5. 编号历史

| 版本 | 说明 |
|---|---|
| v33 | 原名 `v33-handover-2026-10-26`，含四档解密开关。**产物与源码不匹配** |
| **v40** | 由 v33 重命名而来（内容已含雷达/掩体，继续叫 v33 会误导）。补齐产物、重写 VERSION、归档废弃 dex |
| v34~v39 | 跳过，不复用 |

> 为什么跳号：v33 目录里的内容已经不是 v33 了，用连续编号会让人以为中间做了 6 次迭代。
> 跳号能明确表达"这一版是一次版本体系重置"。

## 6. 提交信息规范

```
<类型>: <一句话>

- 具体改了什么
- 为什么改
```

类型：`feat` / `fix` / `docs` / `chore` / `release` / `refactor`

## 7. 分支怎么用

见 `BRANCHES.md`。要点：

- `master` 只接 merge，不直接开发
- 三个方向分支各自只改 `branches/<方向>/`
- 公共内核（`tpen_vN/`、`tpen_app/`、`tpen_kernel/`、`tpen-esp-portable/`）改动先沟通，走 PR

**例外**：版本体系类的改动（重命名版本目录、修正 VERSION、重编产物）属于基础设施，
可直接提交 master，但必须在 `CHANGELOG` 和 `PROGRESS.md` 里留记录。
