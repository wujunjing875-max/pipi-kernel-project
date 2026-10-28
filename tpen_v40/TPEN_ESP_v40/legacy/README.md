# legacy · 废弃产物，勿使用

| 文件 | 说明 |
|---|---|
| `ov_patched_v33.dex` | v33 时期给 Ov50 打过自瞄补丁的 dex（67928 字节）。**不含雷达/掩体模块**，功能比 `dex/classes.dex` 落后一整代。仅作历史留档。 |

需要自瞄版本时：跑 `patch_aim.py` + `patch_aim2.py` 注入到当前 `src/Java/Ov50.java`，再用 `build_ov.sh` 重编，不要直接用这里的文件。
