# v4.2.0 — 幽灵装备 / 人数虚高 / 转视角拖影

> 实机（14:44~15:01，adb 直跑，非 APK）一口气冒出来的四条：
> 1. 显示周围玩家 47 个
> 2. KIRASA 防弹衣之类「不存在但经常绘制」
> 3. 转视角时人机盒子/保险箱拖出很多叠影
> 4. 大厅那个展示人机骨骼又不见了（v4.1.1 已修，本轮回归验证）

本版全部在设备上实测确认过。

---

## 一、玩家数 47 —— 兜底把特效当成人

### 数据

```
t=0 1702 413 83 SGPooledImpactEffect ...  × 29
pl=51
```

`SGPooledImpactEffect`（弹着特效）一批 29 个，全判成真人。

### 根因

v4.1.0 的兜底写成了：

```c
if(kind < 0){
    if(r64(a + 0x330))     kind = 0;   // 有 PlayerState
    else if(tf == 40.0f)   kind = 0;   // ← 灾难：特效/机关/扭蛋机的 tf 也恒为 40
    else                   continue;
}
```

`tf == 40.0f` 是老版本留下的「认不出类名就当玩家」的兜底，
但**特效、机关、扭蛋机这些杂项的 tf 全是 40**，一次能塞进来几十个。

### 修法

```c
if(kind < 0){
    // 三个条件同时成立才算真人：有效骨骼网格 + 骨骼数 >= 20 + 有 PlayerState
    uint64_t m1 = *(uint64_t*)(blk+0x370) & MASK;
    int b1 = m1 ? (int)r32(m1+0x708) : 0;
    if(m1 && b1 >= 20 && r64(a + 0x330)) kind = 0;
    else continue;
}
```

外加一条：**类名已经认出来、但不是 Character/Pawn 的，一律丢弃**（扭蛋机 `BP_GFGashapon_*` 就是靠这条挡掉的，它有 mesh 也有骨骼，只有类名能区分）。

**实测：pl 51 → 1。**

## 二、不存在的 KIRASA 防弹衣

### 数据

```
t=3 1556 629 115 BP_Vest_Kirasa_C   BP_Vest_Kirasa_C_0
t=3 1556 629 115 BP_Helmet_SSh68_C  BP_Helmet_SSh68_C_0
```

两个坐标完全相同、110m 外、**那个位置附近根本没有任何角色**。

### 根因

`BP_Vest_Kirasa_C` / `BP_Helmet_SSh68_C` 是**穿在角色身上的装备 actor**，
不是地上的可拾取物。它们的 RootComponent 坐标在客户端并不可靠
（角色被 cull / 销毁后，装备 actor 还留在某个位置），画出来就是"悬空的头甲"。

### 关键规律（很干净，一眼可分）

| | 类名 |
|---|---|
| 地上真正能捡的 | `BP_9x19mm_PSO_gzh**_Uam_C**`、`BP_556x45mm_M855A1_**Uam_C**` |
| 装备本体（穿着的） | `BP_Vest_Kirasa_C`、`BP_Helmet_SSh68_C`、`BP_Vest_Multicam_C` |

**地上的东西一律带 `_Uam_C`，装备本体一律不带。**

### 修法

```c
if(kind==3){
    int ground = strstr(on,"_Uam_C") || strstr(on,"ItemBase") || strstr(on,"_Item");
    if(!ground) continue;      // 装备本体 / 手持武器，都不是地面物资
}
```

配套还做了距离分层（装备类另外限 35m，其它物资 80m，容器 120m，角色 250m）。

**实测：`BP_Vest_Kirasa_C` / `BP_Helmet_SSh68_C` 从画面消失，只剩 `BP_9x19mm_PSO_gzh_Uam_C`。**

## 三、转视角拖叠影 —— keepAlive 的 key 带位置

### 根因

v4.1.0 的防闪烁缓存这样算 key：

```java
String key = k + "|" + in + "|" + nm + "|" + Math.round(txtX[i] / 45f);
```

**key 里带了屏幕位置格。** 转视角时同一个箱子每帧坐标都在变 → 每帧生成一个新 key →
600ms 内同一个箱子在缓存里堆了十几个条目 → 全部补画 → **拖出一串叠影**。

### 修法

```java
// key 绝对不能带屏幕位置
String key = k + "|" + in + "|" + nm;
static final int KEEP_MS = 250;          // 600 → 250
pvT[j] = now - KEEP_MS;                  // 补画一次就标过期，别连续补
```

实例名在关卡里唯一，位置每帧就地更新，同一个箱子永远只有一条缓存。

## 四、回归验证（设备实测）

```
frame 20700  fps=47  异常数 0
K0=0  K1=2(人机)  K2=0  K3=0
pl=3  p260=0  drawn=2
```

- **连续 20700 帧 0 异常** → v4.1.1 的闪退修复有效（主循环整帧 try/catch 也起了兜底作用）
- 扭蛋机不再出现在 K0
- KIRASA / SSh68 幽灵消失
- 大厅展示人机正常（v4.1.1 修的那条）

## 五、版本

| 文件 | 说明 |
|---|---|
| `paradise_fast58.c` | 兜底收紧 + 只画 `_Uam_C` 地面物资 + 距离分层 + 穿着装备过滤 |
| `Ov43.java` | keepAlive key 去位置 + KEEP_MS 250ms + 只补一次 |

## 六、还没解决的

- **转视角时仍可能有一点滞后感**：采集端一个循环要遍历 ~1450 个 actor 并逐个解析类名，
  一帧可能 30~50ms，期间视角已经转了 → 坐标天然滞后。
  下一步优化方向：给 `cls_name` 加 `actor → 类名` 缓存（同一 actor 的类名不会变），
  预计能把单帧耗时砍掉一大半。
