# Plant V2 流程图

阅读顺序：先看总览，再按需要进入对应细图。每张图只表达一个职责边界，避免跨图连线。

- [总览 SVG](plant_v2_runtime_flow.svg)：六个运行域和故障安全路径。
- [前台行为 SVG](plant_v2_behavior_flow.svg)：触摸/BLE 如何变成语义灯效与振动。
- [传感与生长 SVG](plant_v2_growth_flow.svg)：奖励、位置闭环、生长和萎缩。
- [BLE 与 OTA SVG](plant_v2_ota_flow.svg)：安全门禁、分块写入、校验与重启。
- [生命周期与电源 SVG](plant_v2_power_flow.svg)：状态转换、轻睡眠和恢复性 OTA。

每张 SVG 都由同名 `.dot` 源文件生成。修改后在仓库根目录执行：

```sh
dot -Tsvg 00_docs/architecture/<name>.dot -o 00_docs/architecture/<name>.svg
```
