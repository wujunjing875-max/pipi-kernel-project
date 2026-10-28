# 方案 B · 自瞄联调报告

生成时间：2026-10-26 21:12:09

## 一句话结论

圆形 FOV（r=210px）自瞄链路已跑通：420 帧内锁定率 **95.95%**，
锁定后平均残差 **5.98px（0.4204°）**，
95.04% 的锁定帧误差 ≤8px，开火判定帧 382 帧。

## 运行链路

| 环节 | 实现 | 实测状态 |
|---|---|---|
| 内核层 | 控制设备 `\\.\JianShangAim` IOCTL | **user** — ERROR_ACCESS_DENIED (需要管理员权限，或驱动未允许该访问掩码) |
| 数据源 | tcp://demo-arena（进程外 TCP，模拟跨进程读实体） | 已连接 |
| 自瞄 | circle FOV + 迟滞选靶 + 速度预判 + 指数平滑 | 运行中 |
| 游戏 | 自动拉起 `game/demo_game.py`（144Hz） | 已启动并接管 |

## 自瞄参数

| 参数 | 值 |
|---|---|
| FOV 形状 | 圆形（点到准星距离 ≤ r） |
| FOV 半径 | 210.0 px，面积 138544.2 px² |
| 选靶 | nearest，切换迟滞 ×1.3 |
| 平滑系数 | 0.45 |
| 单帧限速 | 90.0 px |
| 预判 | 22.0 ms |
| 死区 | 1.2 px |
| 反应延迟 | 90.0 ms |
| 开火阈值 | 6.0 px |

## 指标

| 指标 | 数值 |
|---|---|
| 总帧数 / 仿真时长 | 420 帧 / 5.833 s |
| 锁定帧数 / 锁定率 | 403 / 95.95% |
| 首次锁定 | 第 18 帧（250.0 ms） |
| 目标切换次数 | 1 |
| 平均预置误差 | 13.4 px |
| 平均残差 | 5.98 px |
| 残差中位数 | 1.47 px |
| 最大残差 | 217.22 px |
| ≤8px 占比 | 95.04% |
| 开火帧 / 占比 | 382 / 90.95% |
| 墙钟耗时 | 3.833 s |

## 产物

- `out/01_scan.png` 扫描态（目标未进圈）
- `out/02_acquire.png` 捕获瞬间（圈边锁定）
- `out/03_locked.png` 锁定收敛
- `out/04_steady.png` 稳态跟枪
- `out/contact_sheet.png` 四帧对照
- `out/aimbot.gif` 动态回放
- `out/metrics.json` 原始指标

## 接到真游戏

1. 按 `driver/README.md` 用 WDK 编译 `driver/JianShangAim.sys`，管理员加载 → `probe()` 返回 kernel 模式。
2. 填 `offsets.json`（模块名、实体管理器 RVA、链表指针 RVA、数量 RVA、stride、字段偏移）。
3. 把 `run_test.py` 里的 `TcpBackend` 换成 `ProcessBackend(KernelClient(), pid, offsets.json)`，引擎一行不改。
