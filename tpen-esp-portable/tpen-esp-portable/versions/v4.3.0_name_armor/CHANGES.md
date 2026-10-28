# v4.3.0 — 真人名字 + 头甲信息（对齐 RC）

> 需求：RC 会给真人标名字、标头甲信息，我们也要。

## 一、名字：偏移是 `PlayerState + 0x440`，不是 RC 的 0x470

RC 源码里是：

```c
long int objTeam = lsp64(Objaddr + 0x390);
nameAddr = lsp64(objTeam + 0x470);
getUTF8(PlayerName, nameAddr);
```

照抄过来得到的是乱码 `Ⱔ蔳q` —— **RC 是另一个游戏版本的偏移**。

### 实测定位过程

先把 `actor+0x330` 指向的对象类名打出来确认身份：

```
ps=6c7fb3f540 cls=BP_UamPlayerState_C      ← 确认 0x330 是 PlayerState
```

再扫 `PlayerState + 0x300 ~ 0x800`，把所有「像 UTF16 字符串」的指针全打出来：

```
pn=310:兰輢q        ← 乱码
pn=440:家阳朋友     ← ★ 玩家名字
pn=450:家阳朋友     ← FString 的另一份拷贝
pn=470:Ⱔ蔳q        ← RC 的偏移，我们这版是别的东西
pn=6f8:家阳朋友
pn=718:家阳朋友
pn=748:https://thirdwx
```

**结论：名字在 `PlayerState + 0x440`（备选 0x450）。**

### 实现

```c
static int read_pname(uint64_t ps, char *out, int cap){
    uint64_t np = r64(ps + 0x440);
    if(无效) np = r64(ps + 0x450);
    // 读 46 字节 UTF16 → 转 UTF8
    // 空格/制表符替换成 '_'（否则会破坏 esp.txt 的空格分隔）
}
```

输出到 `t=` 行第 12 字段（只对 kind==0 真人，人机没有 PlayerState）。

## 二、头甲信息

采集端本来就有（`armorLv` / `helmetLv`，从装备的耐久曲线资产名 `.../LV3_Kirasa/...` 解析），
一直在 `t=` 行的第 9/10 字段，只是绘制端没显示成文字。

现在标签格式：

```
家阳朋友 头3 甲4 123M        真人（有名字）
玩家 头2 甲3 87M             真人（拿不到名字时）
人机 45M                     人机（AI 没有装备数据，恒不显示）
```

## 三、修 gtext 的 ASCII 宽度（否则英文名字糊成一团）

字形图集里每个字形都是 **30×34 的单元格**，但 ASCII 字符的 advance 只有 8~11。
`gtext()` 原来按单元格宽 30 画，导致英文字母互相压住。

```java
int dw = (g[4] < 20) ? (g[4] + 2) : g[2];   // 窄字符用 advance 当绘制宽度
Object dr = cRect.newInstance(x, y, x + dw, y + g[3]);
```

这样 `gwidth()`（advance 累加）和实际绘制宽度一致，名字才能正常显示。

## 四、顺带：子弹不再绘制

`Bullet_AKM_Uam_C` 带 `_Uam_C` → 通过了「地面物资」判定 → 满屏都是。
加进 `ov_skip.txt` 默认表（`bullet` 关键字）。

## 五、代码变更

| 文件 | 变更 |
|---|---|
| `paradise_fast64.c` | 名字读取（PlayerState+0x440, UTF16→UTF8）+ `t=` 行第 12 字段 |
| `Ov44.java` | 解析名字、`txtPName[]`、标签显示「名字 头X 甲Y 距离M」、gtext ASCII 宽度修正 |
| `apk/srcfiles/skip.txt` | 加 `bullet` |

## 六、验证状态

- 名字偏移**已在实机确认**（扫出 `家阳朋友` 这个名字）。
- 由于验证完那一刻真人全部离开了 cull 范围（`pl=0`），**「名字实际画在屏幕上」这一步还没看到**，
  下次有真人靠近时确认。
- 头甲等级沿用已实机验证过的 `OFF_VEST/OFF_HELM` 自动学习机制。

## 七、RC 本体（`UziUam.rc`）

`UziUam.rc` 是 RC 的 aarch64 PIE 可执行（3.5MB，`PT_INTERP=/system/bin/linker64`），
已 push 到 `/data/local/tmp/UziUam.rc` 并后台运行（PID 2812）。

注意它用的是 **TwT 驱动**（`MY_CALL(0x114514, 0x1919810, 0x2778, &fd)`），
和我们的 paradise 驱动（`0x7a3b9c4d / 0x2f8e1d6b / 0x1234567`）**不是同一套**，
所以它现在读不到内存，只能作为对照看它的 UI 布局。
