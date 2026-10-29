# TPEN ESP · main（v33 正式版）

> 本仓库分支职责划分
>
> | 分支 | 内容 | 用途 |
> |---|---|---|
> | **main**（当前） | **v33-handover-2026-10-26 完整版** | 正式生产版本，clone 即可用 |
> | **HIS** | 仓库历史全部内容（含 abu-aim / js-aim / tpen_app / tpen_kernel / tpen_v40 / workspace 等） | 存档，供追溯与素材取用 |
> | **master** | v40-radar-wall（雷达+掩体开发线） | 开发分支，功能未在真机验证 |

---

## 这是什么

暗区突围（`com.tencent.mf.uam`）的 UE4 内存透视工具 v33 版本。

三层架构：

```
内核驱动（Ko，reboot hook）→ 数据层（bin/p，C 编译）→ 渲染层（dex/classes.dex，Java）
                                        ↓
                                   esp.txt（文本协议解耦）
```

## 版本指纹（务必核对）

```
版本: v33-handover-2026-10-26
采集端 bin/p        md5 44acddd77571e21246b3bb9c3e59600e   39528 字节
绘制端 dex/classes.dex  md5 6a7dfe0ac3ef9fbac8d3779cb07dde55   63796 字节
```

```bash
md5sum tpen_v33/TPEN_ESP_v33/bin/p tpen_v33/TPEN_ESP_v33/dex/classes.dex
# 必须与上面两行完全一致，否则不是这个版本
```

## v33 的功能边界（重要）

**有**：
- 方框 / 骨骼 / 射线 / 距离 / 名称 / 血量显示
- 类型区分：特遣人机 / 普通人机 / 真人
- **加密态破解**：四路解密开关（解密1/2/3/4），走 `SGCharacterFastRepProxy` 的 `+0x314` / `+0x330` / `+0x11050` 三通道取真坐标
- 加密状态检测：`enc3` / `enc4` / `enc5` / `enz` / `zspan` / `dz`
- 假坐标保护、未绘制计数 `undrawn`、物资分类 `cat_*`
- AI 玩家与真人合并显示、名字兜底

**没有**（这些在 master 的 v40 开发线上，未真机验证）：
- 雷达（`drawRadar`，需要采集端输出 `cam=` / `vp=` / `w=`）
- 掩体透视 / 墙体透视（`loadWallData`，需要 `walls/<map>.tcol`）
- 创作者模式

> 判断依据：v33 的 `bin/p` 内**不含** `cam=%` / `vp=%` / `w=%d` 格式串，
> `classes.dex` 内**不含** `drawRadar` / `loadWallData` / `pickAutoMap` / `wQuad` 方法名。
> 所以 v33 不可能有雷达和掩体，这是版本事实，不是配置问题。

## 目录结构

```
tpen_v33/TPEN_ESP_v33/
├── VERSION.txt              版本指纹
├── CHANGELOG-ALL.md         全部版本演进记录（v1 → v33）
├── HANDOVER.md              交接文档
├── bin/p                    采集端二进制（aarch64）
├── dex/classes.dex          绘制端 dex
├── src/C/paradise_fast100.c 采集端源码（1133 行级）
├── src/Java/Ov50.java       绘制端源码
├── drivers/                 6 个内核档位驱动（自解压 sh，base64 内嵌 ko）
│   ├── drv_510.sh  5.10.252-dirty
│   ├── drv_515.sh  5.15.202-android13
│   ├── drv_61.sh   6.1.166-dirty
│   ├── drv_66.sh   6.6.127-4k
│   ├── drv_612.sh  6.12.76-4k
│   └── drv_618.sh  6.18.32-4k
├── assets/                  图标与物资表（g.png/ui.png + 文本）
├── scripts/                 启动脚本（catch_start / start_col / start_ov / deploy_ov）
├── tools/                   31 个逆向辅助工具（C 源码）
├── docs/                    7 篇文档
│   ├── 00-交接速览.md
│   ├── 01-架构说明.md
│   ├── 02-驱动对接.md
│   ├── 03-偏移表.md
│   ├── 04-编译与部署.md
│   ├── 05-踩坑记录.md
│   └── 06-待解决问题.md
├── build_all.sh             一键编译脚本
└── deploy.sh                部署脚本
```

## 快速开始

```bash
# 1. 看架构和偏移
cat tpen_v33/TPEN_ESP_v33/docs/01-架构说明.md
cat tpen_v33/TPEN_ESP_v33/docs/03-偏移表.md

# 2. 确认设备内核档位
adb shell 'uname -r'
# 例 6.1.118-android14-O64646 → 用 drv_61.sh

# 3. 推驱动并加载
adb push tpen_v33/TPEN_ESP_v33/drivers/drv_61.sh /data/local/tmp/drv.sh
adb shell 'su -c "sh /data/local/tmp/drv.sh"'
# 成功输出 ok；失败会打「驱动加载失败」+ dmesg 相关行

# 4. 编译采集端（用 zig，无需 NDK）
cd tpen_v33/TPEN_ESP_v33
export PYTHONPATH="D:/WORKAI/ANQU/DIY/.tools/ziglang"
python -m ziglang cc -target aarch64-linux-musl -O2 -o bin/p src/C/paradise_fast100.c

# 5. 部署（详见 docs/04-编译与部署.md）
sh deploy.sh
```

## 三条铁律

1. **只读，不写游戏内存。** 全部走 `ioctl(P_READ_PHYS)`。
2. **加密态必须开解密**，否则坐标是 `z≈-500` 的假值，会画到地下。
3. **驱动独占。** 同一时刻只能一个进程 attach，被 RC 之类抢占后会 `no pid`，需 `kill -9` 或重启设备。

## 编译环境

| 组件 | 工具链 | 命令 |
|---|---|---|
| 采集端 `bin/p` | zig（项目自带） | 见上方第 4 步 |
| 绘制端 `dex/classes.dex` | 见 `build_all.sh` | `sh build_all.sh` |
| 驱动 `drv_*.sh` | 已内嵌成品 ko | 直接用，无需编译 |

## 路线图

- v33（本分支）：稳定版，加密态破解完整，产物与源码一致，可直接部署
- master / v40-radar-wall：新增雷达 + 掩体透视 + 创作者模式，产物已按源码重编
  （`bin/p` 含 `cam=`/`vp=`/`w=`，`dex` 含 `drawRadar`/`loadWallData`），**待真机验证**

## 许可

内部技术交流用途。
