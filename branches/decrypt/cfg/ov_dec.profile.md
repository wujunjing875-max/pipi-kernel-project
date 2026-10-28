# 解密档位实测记录

每测一档就在下面加一行，别凭记忆。

| 日期 | 档位 | 分辨率 | local 坐标 | 真人贴不贴 | 人机贴不贴 | enc3 | 结论 |
|---|---|---|---|---|---|---|---|
| 2026-10-26 12:52 | dec=1 dec4=0 | 2414x1080 | 正常 | 飘天上 | 飘天上 | -1 | 飘与解密无关，查分辨率 |
| 2026-10-26 12:57 | dec=1 | 2414x1080 | 正常 | 飘天上 | 飘天上 | — | 根因=分辨率错配 |

## 判据备忘

- 人机不需要解密。人机也飘 → 100% 不是解密问题。
- `local=` 出现 `0,0,0` → 采集端没拿到本地坐标，先看 `p.log`。
- `local=` 数值跳变几十万 → 解密档位错（大概率 dec4）。
- `enc3=-1` 伴随飘 → 记下来，别急着改算法。

## 切换脚本（一次改两处，避免只改一半）

```bash
# dec 档
adb shell su -c 'sed -i "s/^dec=.*/dec=1/" /data/local/tmp/ov_cfg.txt'
adb shell su -c 'echo 1 > /data/local/tmp/ov_dec.txt'
adb shell su -c 'echo 0 > /data/local/tmp/ov_dec4.txt'
adb shell su -c 'sh /data/local/tmp/.c/start_col.sh; sh /data/local/tmp/.c/deploy_ov.sh'
```
