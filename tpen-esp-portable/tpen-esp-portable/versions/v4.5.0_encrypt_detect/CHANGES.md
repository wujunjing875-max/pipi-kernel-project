# v4.5.0 — 坐标加密检测 + 全图真实玩家数

> 本轮用户给了一条**决定性信息**：某些账号/对局会被游戏**加密真人坐标**
> （被举报、设备环境异常时触发），此时真人画出来全挤在一个点（地下），
> 而人机坐标不受影响。

## 一、加密判定（对齐 RC）

RC 源码里写得很清楚（`refence/内核源码/jni/include/主程序.h` 610~640 行）：

```c
bool isSelfObject = (selfObj == Objaddr);
bool isRealEnemy = (getFloat(Objaddr + 0x47C) == 40.0f);   // ← 加密标志

if (isSelfObject) {
    pread64(handle, &currentPosition, 12, objTransform + 0x168);   // 自己：直接读
}
else if (EnableDecryption && isRealEnemy) {
    // 解密路径
    uintptr_t Object    = lsp64(Objaddr + 0x180);
    uintptr_t MyRootPtr = lsp64(lsp64(lsp64(Object + 0x118) + 0x0) + 0x90);
    pread64(handle, &decryptedPos, 12, MyRootPtr + 0x1160);        // ← 真坐标在这
}
else {
    pread64(handle, &currentPosition, 12, objTransform + 0x168);   // 盒子/物资：直接读
}
```

**判定式**：`*(float*)(actor + 0x47C) == 40.0f` → 该角色的坐标被加密。

> 注意 RC 的 root 是 `actor+0x180`、tf 是 `+0x47C`；我们这版 root 是 `+0x158`、tf 一直是 `+0x42C`。
> 两个偏移**都要实测确认**，所以本版同时统计两个数：
> - `enc=命中数/真人总数`（0x47C 处 == 40 的个数）
> - `t42=个数`（0x42C 处 == 40 的个数）

### 怎么用这两个数判断

| 场景 | enc | t42 | 结论 |
|---|---|---|---|
| 加密局 | 4/4 | 4 | **0x47C 是加密标志** |
| 加密局 | 4/4 | 0 | 0x42C 才是标志（0x47C 也对） |
| 正常局 | 0/4 | 4 | 0x42C 恒为 40，不能做判据 → 用 0x47C |
| 正常局 | 0/4 | 0 | 两个都不是，需要再找 |

## 二、左上角状态显示（Ov47）

```
坐标 加密中   ← 红色（encHit > 0）
坐标 正常     ← 绿色（有真人且一个都没命中）
坐标 未知     ← 灰色（视野里没有真人，无法判断）
```

位置：屏幕左上角 (20,20)，42px 高，带半透明黑底和顶部色条。

## 三、全图剩余玩家数

之前的 `pl` 只是**客户端 cull 范围内（约 260m）**的真人数，不是全图。

本版改成读 **`GameState.PlayerArray`**（TArray<APlayerState*>，服务器下发的全量玩家列表，
不受 cull 限制）：

```c
// 遍历时找到类名含 GameState（且不是 PlayerState）的 actor
for (og = 0x200; og < 0x500; og += 8) {
    dat = r64(a + og);  num = r32(a + og + 8);  mx = r32(a + og + 12);
    // TArray 结构：Data 指针有效 + 1<=Num<=100 + Max>=Num
    if (dat 有效 && num 合理 && mx 合理) {
        e0 = r64(dat);                        // 第一个元素
        if (类名(e0) 含 "PlayerState") → 这就是 PlayerArray
    }
}
```
偏移是**自动学习**的（`GS_ARR_OFF` 学到一次就固定），输出 `realpl=N`。

绘制端顶部条改成：

```
全图 12   260m 内 2      ← realpl 优先，拿不到才退回旧的 pl
```

## 四、待实测确认（设备没电）

1. 进一局**加密局**（就是刚才那种、真人挤成一坨的），看：
   - `esp.txt` 里 `enc=?/?`、`t42=?`
   - 左上角是否变红"坐标 加密中"
2. 进一局**正常局**，同样看上面两项，应该 `enc=0/N` + 绿色"坐标 正常"
3. 看 `realpl=` 是否等于实际开局人数（20/24 之类），而不是个位数

## 五、后续（加密局怎么画出真人）

RC 的解密链已经抄下来了，只需要确认三个偏移在我们这版的对应值：

```
Object    = r64(actor + 0x158)   // 我们的 root（RC 是 0x180）
MyRootPtr = r64(r64(r64(Object + 0x118) + 0) + 0x90)
pos       = read 12 bytes at (MyRootPtr + 0x1160)
```

下一步做法：对**加密中的真人**，同时打印「旧坐标」和「按 RC 链解出来的坐标」，
如果 RC 链解出来的值合理（跟人机一样的量级、不重叠），就直接切过去。
