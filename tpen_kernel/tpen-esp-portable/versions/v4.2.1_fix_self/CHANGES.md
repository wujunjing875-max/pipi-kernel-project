# v4.2.1 — 「旁边有真人但画不出来」的真正原因

## 一句话

**`0x1290 == 3` 不是「只有自己」**，远端真人也会是 3。我用它排自己，
把附近的真人一起排掉了。

## 现场数据（2026-10-25 15:05，fast58d）

同一个快照里 4 个 `BP_UamCharacter_C`：

```
d=85 ua=2 ps=6c7fbdcd20 mesh=6c7f8efd20 bones=109  真人
d=87 ua=2 ps=6c7f8de540 mesh=6c7f9382a0 bones=109  真人
d=85 ua=3 ps=6c8013ea80 mesh=6c7fc022a0 bones=109  真人  ← 被当「自己」排掉
d=90 ua=3 ps=6c7f7da2a0 mesh=6c7f9cb2a0 bones=109  真人  ← 被当「自己」排掉
```

`ps`（`actor+0x330` PlayerState）全部非空 → 四个都是真人。
但 `ua` 有两个是 2、有两个是 **3**。

## 错在哪

v4.1.0 我写了这句：

```c
{ uint32_t us = r32(a + 0x1290);
  if(kind==0 && us==3) continue; }   // 以为 3 == 自己
```

结果 85m、90m 那两个真人被当成"自己"直接 continue 掉了。
用户看到的就是「旁边明明有真人，ESP 一个都没画」。

## 修法

```c
// 唯一可靠判据：dm == 0（和本地 RootComponent 完全重合）
// 敌人不可能和你在同一个坐标上；1m 起的队友照画
if(dm == 0) continue;
```

## 验证（15:12 实机）

```
frame 46500 fps=57
K0=4/BP_UamCharacter_C     ← 4 个真人正常画出
K1=7/BP_UamAICharacter_C
K2=6/Inventory_071_SafeBox_C
K4=1/BP_UamCharacter_C
pl=4  p260=4  drawn=4
```

之前是 `K0=0`、`drawn=0`。

## 复盘：为什么会犯这个错

14:04 那次只抓到一个 `d=0 ua=3` 的对象（当时只有自己），就下了「3 = 只有自己」的结论。
**样本只有 1 个就归纳，是这次的直接原因。** 以后这类枚举值判定至少要抓到 3 个以上样本、
且包含"远端同类对象"才能定论。
