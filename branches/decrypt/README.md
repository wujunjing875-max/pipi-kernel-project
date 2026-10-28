# 解密分支（feat/decrypt）

管的是**坐标解密档位**：`ov_cfg` 里的 `dec` / `dec2` / `dec3` / `dec4` 四个开关，同一时刻只能有一个为 1。

## 四档含义

| 开关 | 叫法 | 现状 | 表现 |
|---|---|---|---|
| `dec=1` | 解密1 | **当前使用** | 真人坐标正常，README 主线基线 |
| `dec2=1` | 解密2 | 未启用 | 未验证 |
| `dec3=1` | 解密3 | 未启用 | `enc3=-1` 时与解密4 一起出过问题 |
| `dec4=1` | 解密4 | **禁用** | 真人坐标飘天上（错误解密） |

规则一句话：**只开一个，`dec=1` 是安全档，`dec4` 是已知坏档。**

## 现场操作

配置文件在设备上：`/data/local/tmp/ov_cfg.txt`（也有独立的 `ov_dec.txt` / `ov_dec4.txt` 开关文件），改完必须重启 overlay 才重读：

```bash
# 改档位（设备端执行）
adb shell su -c 'sed -i "s/^dec=.*/dec=1/; s/^dec4=.*/dec4=0/" /data/local/tmp/ov_cfg.txt'

# 重启：先停 overlay 和采集端，再一键起
adb shell su -c 'sh /data/local/tmp/.c/start_col.sh'
adb shell su -c 'sh /data/local/tmp/.c/deploy_ov.sh'
```

`tpen_v33/TPEN_ESP_v33/scripts/` 下有 `start_col.sh`（采集端）、`start_ov.sh` / `deploy_ov.sh`（overlay），整链在 `tpen-esp-portable/.../tools/start_esp.sh`。

## 验证（这一步有坑，务必照做）

```bash
# 正确：纯设备端读，$() 不在本机展开
adb shell su -c 'cat /data/local/tmp/ov_dec.txt'

# 错误：本机会先把 $(...) 展开，读到的是本机路径，误报 No such file
echo dec1=$(cat /data/local/tmp/ov_dec.txt)
```

判断解密是否生效看两件事：
1. `esp.txt` 里 `local=` 的坐标是否合理（不是 `0,0,0`，也不是几万级的跳跃值）
2. 屏幕上真人位置是否贴合模型 —— **但飘天上不一定是解密问题**，先看分辨率。

## 关键结论（别再踩）

- **人物飘天上的根因是分辨率错配，不是解密。** 判据：人机（本来就不需要解密）也飘 → 排除解密。
- 解密4 切解密1 之后仍飘 → 不是档位问题。
- `enc3=-1` 是异常值，出现在解密4 状态下，跟坐标飘一起出现。

## 这条分支能改什么

- `cfg/ov_dec.profile.md` —— 四档实测记录模板
- 只动本目录。要改 `Ov50.java` 里的解密算法，走 master PR。
