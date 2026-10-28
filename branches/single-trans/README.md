# 单透分支（feat/single-trans）

单透 = **只渲染指定一类的目标**，其它全部关掉。区别于"全透"（玩家/人机/容器/物资一起画），单透只保一类，画面干净、CPU 占用低、被截图核对时特征也少。

## 链路（谁产数据、谁画）

```
drv.sh（驱动）
   ↓
.c/p（采集端，cmdline 被抹成 kworker，只能靠 /proc/*/exe 链接找）
   ↓  写 /data/local/tmp/esp.txt
Ov50 overlay（app_process 起，读 esp.txt 渲染）
```

启动总入口：`tpen-esp-portable/tpen-esp-portable/tools/start_esp.sh`
（先让旧 overlay 自杀 → 杀旧采集端 → 加载驱动 → 起 overlay → 起采集端）

## esp.txt 数据格式（实测抓的）

```
pid=19702
ts=1792923143027
cnt=1641
local=-6075,9538,117
a=45 1768 514 17 35 66
t=2 1768 489 66 Inventory_071_SafeBox_C Inventory_071_SafeBox_C_0 0 0
c=Inventory_071_SafeBox_C
s=1056 307 1060 306
```

| 行 | 含义 |
|---|---|
| `pid` / `ts` / `cnt` / `local` | 进程号 / 时间戳 / 目标总数 / 本地坐标 |
| `a=<id> <x> <y> <w> <h> <dist>` | 屏幕方框：左上角、宽高、距离 |
| `t=<type> <x> <y> <h> <蓝本> <实例> ...` | 目标身份，`type` 决定类别 |
| `c=<类名>` | 紧凑类名，用于类别过滤 |
| `s=<x1> <y1> <x2> <y2>` | 线段（骨骼 / 轮廓），只有非加密热更态才有 |

两种现场状态（快照分别在 `../tpen-esp-portable/tpen-esp-portable/snapshots/`）：

- `enc_on/esp_enc_on.txt` —— 加密态，count 大（1600+），只有 `a/t/c`，**没有 `s=` 线段**
- `noenc_hot/esp_noenc.txt` —— 非加密热更态，count 小（1200+），**带 `s=` 线段**

单透优先走 `noenc_hot` 那套：有线段就能画骨骼，只挑一类目标，渲染量最小。

## 类别开关（ov_cfg.txt）

```
cat_n_box_player=1   # 玩家  ← 单透通常只开这一个
cat_n_box_bot=1      # 人机
cat_n_safebox=1      # 保险箱
cat_n_med / ammo / weapon / tool / bag / drawer / wardrobe / jacket / pc / file = 0
```

只留 `cat_n_box_player=1`，其余全 0，就是最干净的单透。

## 渲染开关（ov_cfg.txt 里的）

| 键 | 作用 |
|---|---|
| `master=1` | 总开关 |
| `k0`~`k4` | 快捷功能键 |
| `bone=1` | 骨骼线（依赖 `s=` 线段，加密态没有） |
| `lab=1` | 名字标签 |
| `abar=1` | 血条 |
| `fps=144` | 渲染帧率上限 |
| `rot=0` | 旋转/朝向 |
| `aff=...` | 视角矩阵参数（6 个浮点），换分辨率必须重算 |

## 已知坑

- **分辨率错配会让人物飘天上**，不是解密问题（详见 decrypt 分支）。`start_esp.sh` 里 overlay 启动带 `2414 1080` 两个参数，跟平板实际分辨率必须对上。
- 加密态没有 `s=`，`bone=1` 画不出骨骼——只能画方框。要骨骼就必须走非加密热更。
- 采集端进程名被抹成 `kworker`，`ps | grep p` 找不到，杀进程只能遍历 `/proc/*/exe`。

## 这条分支能改什么

- `cfg/ov_cfg.single-trans.ini` —— 单透推荐配置（本目录内）
- 只动本目录。要改 overlay 的渲染逻辑（`tpen_v33/TPEN_ESP_v33/src/Java/Ov50.java`），走 master PR。
