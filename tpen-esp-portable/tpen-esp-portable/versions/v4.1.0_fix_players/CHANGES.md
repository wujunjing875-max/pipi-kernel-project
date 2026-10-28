# v4.1.0 — 玩家/队友漏画、盒子误判、整片闪烁、人数乱跳

> 触发问题（用户实机反馈，4 条）：
> 1. 面前有真人玩家但没绘制出来
> 2. 全图剩余玩家人数一直在变化（乱跳）
> 3. 一片区域的绘制一直闪烁，另一片不会
> 4. 没有队友的绘制；面前两个高级人机盒子被画成「玩家盒子」

本版全部为**离线代码修复**，上一轮设备没电前已抓到关键现场数据，结论如下。

---

## 一、真凶数据（2026-10-25 14:04 现场抓）

用 `paradise_fast48d`（DBGCHR 版）打印每个 Character 类 actor 的原始字段：

```
d=0    ua=3 ps=687dbb1540  bones=109  BP_UamCharacter_C      ← 自己
d=219  ua=2 ps=687dd232a0  bones=109  BP_UamCharacter_C      ← 真人（队友/敌人）
d=158  ua=1 ps=0           bones=109  BP_UamAICharacter_C    ← 人机
d=105  ua=2 ps=0           bones=109  BP_UamAICharacter_C    ← 人机玩家
d=4    ua=3 ps=?           tf=40.0    BP_UamCharacterTemplate_Preview_C  ← 大厅展示台
```

**结论（和 RC 同源）**：

| 判据 | 含义 |
|---|---|
| `actor + 0x330`（PlayerState）**非空** | **真人玩家**（队友/敌人） |
| 同上为空 + 类名含 `AICharacter` | 人机 / 人机玩家 |
| `actor + 0x1290 == 3` | **只有本地自己** |
| `actor + 0x1290 == 2` | 玩家模型的远端单位（真人 + 少数 AI） |

RC 源码里是 `isBot = getDword(actor + 0xE10) == 256` 只分人/机（偏移对不上我们这版），
但同一个思路：**靠 PlayerState 判「人」，不靠类名/枚举**。

## 二、四条问题的根因与修法

### 1) 真人/队友不画 —— 两个独立 bug 叠加

**bug A：分类错。** 旧逻辑 `ua==2 → kind=4（人机玩家）`，而 `ua==2` 恰好是**所有远端玩家**
（队友和敌人）的枚举。于是队友被归进「人机玩家」，用户关了那个开关就一片空白。

修：`pst != 0 → kind = 0`（真人）；`pst == 0 且 !modelAI → kind = 0`（玩家模型一律当真人，
宁可多画不可漏画）；`pst == 0 且 modelAI → kind = 4/1`。

**bug B：排除自己用的是距离。** 旧代码 `if(dm < 2 || dm == 0) continue;`，
而 `dm` 是用 `local`（本地 RootComponent 坐标）算的，和角色真实站点有 0.5~1m 偏差,
**队友贴身站时 dm 直接算成 0，被当成「自己」排掉了**。

修：改用 `0x1290 == 3` 排自己，不再用任何距离阈值。

**bug C（兜底）**：类名解析失败的 actor 以前只在 `tf==40` 时保留。现在先看 PlayerState：
有 PlayerState 就一定是真人，直接画。

### 2) 剩余人数一直变化

旧统计：`kind==0 || kind==4` 且 `ua∈{2,3}` —— 把「人机玩家」全算进去了，
人机刷新一次数字就跳一次。

修：**只数 `kind==0`**（= 有 PlayerState 的真人），并且排除自己（`ua==3`）。
实测修复后连续 12 秒稳定在 `pl=1`，之前是 1→3→8 乱跳。

绘制端再加**1.2 秒滞后**（`settlePl()`）：新值必须连续 1.2 秒不变才采用，
避免偶发丢帧那一下的闪跳。

### 3) 一片区域整片闪烁 —— 三个原因

**原因 A：绘制距离压到 cull 边界。** 上一版把绘制上限放到 260m，而客户端 cull 正好约 260m，
边界上的物件每帧存在性不同 → 整片闪烁。

修：分层距离 —— **角色 250m / 容器·物资 120m**（留 10m 余量）。

**原因 B（关键）：esp.txt 读失败会把整屏清空一帧。**
原来 `boxN=0; txtN=0;` 在 `loadEsp()` **之前**执行，而 `loadEsp()` 里 `new FileReader(esp.txt)`
可能在采集端 `rename()` 的瞬间抛 `ENOENT` → catch 掉 → 这一帧标签数组全是 0 → 闪一下。

修：把清空动作移进 `loadEsp()`，**文件打开成功之后**才清。打开失败就整帧沿用上一帧。

**原因 C：容器/物资客户端 streaming 本来就会抖。**
新增 `keepAlive()`：容器/物资按「kind + 实例名 + 类名 + 位置格(45px)」登记，
本轮没出现但 600ms 内的，用上一帧坐标补画一次。

### 4) 高级人机盒子被标成玩家盒子

`BP_DeathBox_Gendarmerie_C`（宪兵 = 高级 AI）不命中任何人机关键词
（旧表只有 scav/bot/npc/ai），于是落到「默认玩家盒」。

修：**规则反过来 —— 默认人机盒，只有类名里明确有 `player` / `pmc` 才算玩家盒。**
同时把 `ctype.txt` 表也改了（表是「最长关键字优先」，所以要把 `deathboxplayer` 写在 `deathbox` 前面）：

```
deathboxplayer n_box_player
deathboxpmc    n_box_player
playerbox      n_box_player
deathbox       n_box_bot      ← 默认人机
corpse         n_box_bot
rewardbox      n_box_player
...
gendarmerie    n_box_bot
rogue          n_box_bot
raider         n_box_bot
```

### 5) 顺手修：大厅展示台被当成真人

`BP_UamCharacterTemplate_Preview_C` 身上 `ua` 也是 3、`bones` 也够 109，
靠字段完全分不出来 —— 唯一可靠的判据是类名里的 `Template` / `Preview`。

修：`isTmpl → continue`（直接丢弃，不再走第二套骨骼校验）。

---

## 三、代码变更

| 文件 | 变更 |
|---|---|
| `01_src_C/paradise_fast52.c` | 分类改 PlayerState 优先；`0x1290==3` 排自己；人数只数真人；距离分层 250/120；Template/Preview 直接丢 |
| `02_src_Java/Ov41.java` | ① `keepAlive()` 容器物资 600ms 保留 ② `settlePl()` 人数 1.2s 滞后 ③ 标签清空移到 `loadEsp()` 内部 ④ 标签数组 32→128 ⑤ 盒子分类默认人机 |
| `apk/src/com/android/calc3/Main.java` | 全新界面（见 v4.2.0 说明），启动类改 `Ov41` |

## 四、验证状态

- 设备在 14:19 没电，本版**未做最终实机回归**。
- 已实机验证的部分（14:04~14:12）：
  - `ps` 判据成立（真人 ps 非空、人机 ps=0）
  - 人数从乱跳变为稳定 `pl=1`
  - 盒子表加载 35 条无报错
- 待验证：队友贴身绘制、两片区域闪烁对比、人数长时间稳定性。

## 五、如果队友还是不画 —— 下一步方向

若下次开机仍然看不到队友，说明**队友 pawn 根本不在 `Level.Actors` 数组里**
（部分 UE4 项目把远端 pawn 挂在 `PlayerController.Pawn` 下）。
届时的验证方法：遍历 `GameState.PlayerArray`（`GameState + 0x2F8/0x300`），
从每个 `PlayerState + 0x30C`（PawnPrivate）取 pawn 再走同一套投影。
`paradise_fast52.c` 里留了 `DBGCHR` 开关，编一份 `DBGCHR=1` 的二进制即可打印
所有 Character 的 `ua/ps/mesh/bones/tf`，一眼能看出来队友到底在不在数组里。

---

## 六、同版 APK 界面改造（对应「Tpen公益 / 全部驱动 / 刷驱动 / 按钮」需求）

### 1) 悬浮窗标题
`Ov41` 里 `text(c, "UZI ESP", 20, 16, 4, C_TEXT)` 换成 `label(c, "brand", 16, 10, 0xFFF0F0F0)`，
`brand` 是 `05_tools/gen_ui.py` 新加的 34px 图集项（文字 `Tpen公益`，实测 149×36，
正好落在 `TITLE_H=60` 的标题区，且不和右边的「内核自研」重叠）。
> 注意：不能用 `gtext()` —— 它的 ASCII 字形是 30px 单元格配 8~11px advance，字母会叠在一起。

### 2) 6 个驱动全部打进 APK
```
drv_510.sh → drv510.bin     drv_66.sh  → drv66.bin
drv_515.sh → drv515.bin     drv_612.sh → drv612.bin
drv_61.sh  → drv61.bin      drv_618.sh → drv618.bin
```
APK 体积 3.69MB → **5.80MB**。全部 AES-256-CBC 加密，asset 名不带版本语义。

### 3) 刷驱动 UI
- **下拉框**选 5.10 / 5.15 / 6.1 / 6.6 / 6.12 / 6.18 → 「刷入选中」
- **「智能刷入」**：读 `System.getProperty("os.version")`（失败回落 `uname -r`），
  取 `major.minor`，匹配顺序 **精确 → 同 major 不超过它的最大 → 全局不超过它的最大 → 最近的**。
  实测本机 `6.1.118-android14-O64646` → 选中 `6.1`。
- 每个版本刷完都会回读 d.sh 的 `ok` / `驱动加载失败` 并打到日志，不匹配就换一个版本刷。

### 4) 启动 / 退出按钮
- **「启动 ESP」**：解密 11 个资产 → `su -c cp` 到 `/data/local/tmp` → 清旧实例 →
  `lsmod | grep paradise` 判断驱动是否在位，不在位就按内核**智能刷一次** →
  起 overlay(`Ov41`) → 等 3.5s → 起采集端 → 等 2.5s → 回读 `p.log`/`esp.txt`。
- **「退出 ESP」**：`touch ov_kill` 让 overlay 自杀；采集端 cmdline 被抹成 `kworker`，
  只能扫 `/proc/*/exe` 找 `*/.c/p` 再 `kill -9`。

### 5) 顺手修的打包坑（重要）
**Python 在 Windows 上 `io.open(p,'w')` 会把 `\n` 转成 `\r\n`** —— 我又一次把 `pack.sh`
写回 CRLF，导致 `rm -rf build` 实际删的是 `build\r`，旧 class 残留（`Main$Task` 等），
d8 报 `Invalid output: build\dex`。

以后改 `.sh` 必须 `io.open(p,'w',encoding='utf-8',newline='')`，或改完补一条
`sed -i 's/\r$//' xxx.sh`，并在打包前 `rm -rf apk/build`。
