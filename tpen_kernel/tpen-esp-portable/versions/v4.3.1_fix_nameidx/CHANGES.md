# v4.3.1 — 名字字段索引写错了（这才是「看不到名字」的真因）

## 一句话

`t=` 行一共 **11 个字段**，名字是 `tp[10]`。
我在 Ov44 里写成了 `if (tp.length >= 12) txtPName[txtN] = tp[11];` ——
长度永远到不了 12，**这行永远不执行**，所以名字一个都没画出来。

## t= 行的真实字段顺序

```
t=0 1693 596 79 BP_UamCharacter_C 2 4000 SGCharacterFastRepProxy_0 0 0 战神PCC
   ↑  ↑    ↑   ↑  ↑                 ↑ ↑    ↑                        ↑ ↑ ↑
 kind cx   y  dm  cls              ua tf100 wp(手持)                甲 头 名字
tp[0]  [1] [2] [3] [4]            [5] [6]  [7]                      [8][9][10]
```

采集端格式串：`t=%d %.0f %.0f %d %s %u %d %s %d %d %s`
→ kind, cx, y, dm, cls, ua, tf100, wp, armorLv, helmetLv, name

## 修法

```java
if (tp.length >= 11) txtPName[txtN] = tp[10];   // 原来是 >=12 / tp[11]
```

头甲的两个索引 `tp[8]`（甲）/`tp[9]`（头）本来是对的。

## 附带发现

- **RC（`UziUam.rc`）会抢占驱动**：它一跑，我们的采集端就拿不到 fd，
  静默退出（只在 stderr 打一行 `no pid`），esp.txt 停止更新，
  overlay 还在画上一帧的旧数据 → 看起来像「只有 RC 在画」。
  驱动是独占的，**RC 和我们的采集端不能同时跑**。要切回我们必须先 `kill UziUam.rc`。
- 驱动被 RC 弄坏后重新 `insmod` 也可能不恢复，**重启设备最干净**。
- 采集端启动可靠性：`nohup ... &` 在 `su -c` 退出时会连子进程一起被杀，
  必须用 `setsid sh -c "exec ... " &`。已写进 `07_docs/start_esp.sh`。

## 待办

- 头甲等级目前读出来还是 0。`OFF_VEST=0x8b8` 能学到（`LV4_Trooper`），但
  `OFF_HELM` 没学到。下一步按 RC 的 ArmorManager 链验证：
  `actor+0x1b38 → +0x1D8 → +0x30(头盔) / +0xF0(护甲)`。
