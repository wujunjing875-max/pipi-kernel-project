# 自瞄 TODO

- [ ] 写 `MemAimProvider`：UWorld → Actors → Head 屏幕投影，替换像素找色
- [ ] 血条颜色阈值做成配置项（换皮肤不用改代码）
- [ ] 平滑系数做成可调，实测找手感区间
- [ ] js-aim 驱动在目标系统上的加载验证（测试签名 + `sc create`）
- [ ] `core/kernel.py` 的 `probe()` 判 kernel 模式是否稳定，失败回退路径写清楚
- [ ] 锁点偏移按方框高度比例算，去掉写死像素
- [ ] 自瞄开关做成运行时热切（不重启 overlay）
