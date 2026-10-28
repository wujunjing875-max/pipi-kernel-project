# CHANGELOG · v40-radar-wall-2026-10-29

> 版本号从 `v33-handover-2026-10-26` 重命名为 **`v40-radar-wall-2026-10-29`**。
> 版本迭代规则恢复为本地惯例：`vN-<主题>-<日期>`，每版独立目录 + 本文件记录 + 产物随版本落盘。

---

## 0. 为什么是 v40

仓库里的 `tpen_v33` 虽然目录名还叫 v33、`VERSION.txt` 也还写着 v33 的 md5，
但**内容早已不是 v33**：采集端多 16 行、绘制端多 708 行，加载了两个全新模块（雷达、掩体透视）。

继续叫 v33 会造成三个问题：

1. 与本地 `tpen-work/v33-handover-2026-10-26` 同名不同物，无法判断哪个是新
2. `VERSION.txt` 里的 md5 与任何实际文件都对不上
3. 无法按版本追踪后续迭代

因此统一跳号到 **v40**，中间编号留空，避免与本地已有 v34~v39 猜测冲突。

---

## 1. 采集端改动（`src/C/paradise_fast100.c`，1117 → 1133 行）

### 1.1 新增 `cam=` 相机视图投影矩阵（16 个 float）

```c
fprintf(f,"cam=%.4f,%.4f,...,%.4f\n", M[0]..M[15]);
fprintf(f,"vp=%.0f,%.0f\n", CX*2.0f, CY*2.0f);   // 屏幕基准尺寸
```

- 用途：绘制端用它做**真正的 3D 世界→屏幕投影**，而不是只靠采集端算好的 2D 点
- 这是掩体/碰撞网格消隐的基础：需要把地图里的三维线段投影到屏幕
- `vp=` 给出投影基准分辨率（2414×1080），绘制端据此算缩放 `SX/SY`

### 1.2 新增 `w=` 雷达世界坐标行

```c
fprintf(f,"w=%d %.0f %.0f %.0f %d %u\n", kind, q[0], q[1], q[2], dm, uaw);
```

- 格式：`w=<类别> <x> <y> <z> <距离> <ua>`
- **关键设计**：这段代码被放在「投影 / 屏幕裁剪」**之前**
  → 身后、视野外、屏幕外的目标也会被输出，雷达才有意义
- 旧版本没有这个字段，雷达无从获取屏幕外目标

### 1.3 骨骼段格式改为带索引

```c
// v33（旧）
fprintf(f,"s=%.0f %.0f %.0f %.0f\n", x1,y1,x2,y2);
// v40（新）
fprintf(f,"s=%d %.0f %.0f %.0f %.0f\n", i, x1,y1,x2,y2);
```

- 新增首个字段 `i` = 骨骼索引
- 意义：绘制端终于能区分"哪一段是哪根骨头"，可以按骨骼类型上色 / 只画特定骨架
- **这是 esp.txt 协议的不兼容变更**，旧绘制端读新采集端的 `s=` 行会解析错位

### 1.4 骨骼名额 4 → 8，且失败不占名额

```c
- if (boneActors < 4 && bones >= 20 && bpos) {
+ if (boneActors < 8 && bones >= 20 && bpos) {
...
+ int emitted = 0;
+ if (emitted > 0) { boneActors++; boneEnter++; }   // 投影全失败的角色不占名额
```

- 名额翻倍：同屏能出更多角色的骨骼
- 修复：以前某个角色骨骼投影全失败仍占掉一个名额，导致后面的角色画不出来

### 1.5 采集间隔 8000µs → 2000µs

```c
- usleep(8000);   // ~60Hz 上限
+ usleep(2000);   // 降低采集等待，实际频率仍受内存读取耗时限制
```

- 采集循环等待缩短 4 倍，理论上限从 ~60Hz 提到 ~200Hz
- 实际帧率受物理内存读取耗时限制，但**加密态坐标延迟会明显改善**
- 代价：CPU 占用上升，注意发热和掉帧

---

## 2. 绘制端改动（`src/Java/Ov50.java`，2088 → 2796 行，净增 708 行）

### 2.1 新模块：掩体 / 墙体透视（wall）

| 新增方法/字段 | 作用 |
|---|---|
| `loadWallData()` | 读取 `walls/<map>.tcol`，校验 magic `TCOL3`，解析线段表 |
| `wQuad` / `wAdd` / `wTile` / `wTileIdx` / `wTiles` | 空间分块索引，按视野裁剪后只投影可见线段 |
| `wBandOf` / `wBandLo` / `wBandHi` | 按高度带筛选墙体，避免把地板当天花板画 |
| `projW` / `projSeg` / `line` | 世界线段→屏幕线段投影 |
| `pickAutoMap()` / `wallAuto` / `wAutoX` / `wAutoDone` | 自动识别当前地图，加载对应 tcol |
| `wRange` / `wNear` / `wBudget` / `wStroke` / `wallColor` | 距离、近裁剪、绘制预算、线宽、颜色 |

配置项（`ov_cfg.txt`）：

```ini
wall=1                # 掩体透视开关
wstyle=0              # 样式
wrange=80             # 距离(米)
wbudget=2000          # 每帧线段预算
wband=0,300           # 高度带
wcolor=00FF88          # 颜色
wnear=30              # 近裁剪
map=auto              # auto=自动识别地图
```

数据依赖：`/data/local/tmp/walls/<地图名>.tcol`（仓库 `workspace_11-45/_walls_backup/` 下有 8 张图，共 43.5MB）

### 2.2 新模块：雷达（radar）

| 新增方法/字段 | 作用 |
|---|---|
| `drawRadar()` | 圆形雷达底板 + 目标点 + 朝向扇形 |
| `camM[3]` / `camM[7]` | 从 `cam=` 矩阵取相机前向，雷达可随视角旋转 |
| `locX` / `locOk` | 自身坐标有效性（雷达需要原点） |
| `radCx/radCy/radR` | 雷达圆心与半径 |
| `radRange` / `radRot` / `radBg` / `radPlayers` / `radBots` / `radLoot` | 范围、是否旋转、背景、显示哪些类别 |

配置项：

```ini
radar=1               # 雷达开关
radrange=100          # 范围(米)
radrot=1              # 随视角旋转
radbg=180             # 背景透明度
radpos=180,180,150    # 圆心x,圆心y,半径
radwho=1,1,0          # 玩家,人机,物资
```

### 2.3 新增模块：创作者模式

```ini
# /data/local/tmp/ov_creator  内容 "1" = 画面板；其它 = 只画 ESP 不画面板
```

`loadCreator()` 读取该文件，控制是否绘制设置面板。适合边玩边录屏时隐藏 UI。

### 2.4 其他新增

- `aff=` 6 参数仿射矩阵配置（`A_a..A_f`），换分辨率后重算用
- `cat_*` 物资分类开关
- `le32()` 小端读取工具（读 tcol 用）

---

## 3. esp.txt 协议变更（★ 不兼容）

| 行 | v33 | v40 | 兼容性 |
|---|---|---|---|
| `cam=` | 无 | `cam=16个float` | 新增，旧绘制端忽略即可 |
| `vp=` | 无 | `vp=宽,高` | 新增 |
| `w=` | 无 | `w=kind x y z dm ua` | 新增 |
| `s=` | `s=x1 y1 x2 y2` | **`s=i x1 y1 x2 y2`** | **不兼容**，多一个字段 |
| 其余（`local=` `t=` `pl=` `enz=` `enc3=` `undrawn=` `pdt=` `fake=`） | — | 不变 | 兼容 |

**结论：v40 的采集端必须配 v40 的绘制端，不能混装。**
混装的后果是骨骼线段解析全部错位（每 5 个数字当 4 个用）。

---

## 4. 本次修正的问题（仓库原有状态）

### 4.1 ★ 产物与源码不匹配（严重）

检查发现仓库里的 `bin/p` **根本不是用这份源码编的**：

```
源码里有 cam= / vp= / w=     →  编译产物里一个都没有
bin/p 内嵌格式串: 只有 s=%d、local=、enz=、enc3=、undrawn=、pdt=
```

也就是说：**拿仓库的 `bin/p` 上机，雷达和掩体透视永远不会出现**，因为采集端压根不输出 `cam=` 和 `w=`。
这很可能就是 `PROGRESS.md` 里「加密态骨骼验证 ⬜ 加密态无 s= 线段」这条一直卡着的原因之一。

**本次处理**：用仓库源码 + 本地 NDK 重新编译，产物已验证含新格式串：

```
out/p → bin/p   40024 字节   md5 639a00a8…   cam=% ✓ vp=% ✓ w=%d ✓ s=%d ✓
```

### 4.2 绘制端产物同步重编

```
out/dex/classes.dex → dex/classes.dex   81912 字节   md5 d08ad455…
已验证含 drawRadar / loadWallData / pickAutoMap / wQuad
```

### 4.3 `VERSION.txt` 失效 → 已重写

旧内容声明的 md5 是本地 v33 的值，与实际文件完全不符。已改写为 v40 的真实 md5。

### 4.4 `ov_patched.dex` 是废弃产物（注意别用）

```
ov_patched.dex  67928 字节   含 dec4On/undrawnN，但不含 drawRadar/loadWallData
```

它是**在加雷达/掩体之前**打的自瞄补丁 dex，功能比 `dex/classes.dex` 落后一整代。
已移到 `legacy/ov_patched_v33.dex`，别拿它上机。

### 4.5 `Ov50.java.bak_aim` 是个空备份

```
Ov50.java          7775535ad514e1c7611af5302c882f46
Ov50.java.bak_aim  7775535ad514e1c7611af5302c882f46   ← 完全相同
```

`branches/aimbot/README.md` 称它是"带自瞄的旧备份"，实际 md5 与当前源码一模一样，**里面没有自瞄代码**。
自瞄只存在于 `patch_aim.py` / `patch_aim2.py`（运行时注入）和已废弃的 `ov_patched.dex`。
要恢复自瞄版本，得重新跑 `patch_aim.py` 再编，不能指望这个 bak。

---

## 5. 升级影响 / 兼容性

| 项 | 说明 |
|---|---|
| 采集端 | 必须重新推 `bin/p` 到设备，旧的不输出 cam/w |
| 绘制端 | 必须重新推 `dex/classes.dex`，否则读 `s=` 会错位 |
| 地图数据 | 掩体透视需要 `/data/local/tmp/walls/*.tcol`，否则只有雷达能用 |
| 配置 | 新配置项都有默认值，`ov_cfg.txt` 不写也能跑（掩体/雷达默认关） |
| 面板 | 新增 wall / radar 开关，位置见 config 默认值 |

---

## 6. 遗留待办（对齐 PROGRESS.md）

1. **加密态骨骼无 `s=` 输出** —— 换成新采集端后优先复测，怀疑是旧 bin 导致的
2. **dec4 已判废**（坐标飘天上），不要再用；基线是 dec1
3. **aff 矩阵重算脚本** 未写，换分辨率必须手算
4. **MemAimProvider** 未实现（自瞄只有像素找色版）
5. **仓库体积** 418MB / 500MB 上限，APK/zip 需清理（tcol 是功能数据，不能删）
