# 解密 TODO

- [ ] 补测 `dec2=1` 单开时的坐标表现，填进 `cfg/ov_dec.profile.md`
- [ ] 补测 `dec3=1` 单开，重点看 `enc3` 是否变 -1
- [ ] 确认 `ov_dec.txt` 与 `ov_cfg.txt` 的 `dec=` 哪个优先（两处都写会不会互相覆盖）
- [ ] 把"分辨率错配导致飘天上"的排查步骤写成脚本（读 overlay 启动参数 vs `wm size` 实际值）
- [ ] `aff` 矩阵在不同分辨率下的重算，跟 single-trans 分支对一下，别各写一套
- [ ] 记录 `local=` 正常值的合理范围（判断解密对不对的硬标准）
