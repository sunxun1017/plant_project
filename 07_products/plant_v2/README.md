# Plant V2 产品组合

本目录把 V2 BSP、ESP-IDF Adapters、业务 Services 和协议 V2 组合为 `Plant-V2-C3` 固件。
从仓库根目录构建：

```sh
source /home/sx/develop/esp/esp-idf/export.sh
idf.py -B build-v2 -DPLANT_PRODUCT=v2 build
```

V2 使用普通共阴 5050 RGB 三路 PWM 变体；若原理图改用 WS2812B，必须新增对应 RMT
Adapter 并在 BSP 中切换，不能只把三路 PWM 引脚合并成一个数据脚。

交叉编译只证明 ESP-IDF API、配置和链接一致。三路 ADC 阈值、舵机/电位器方向与安全端点、
ECM 包络、GL5528 明暗阈值、AHT21 安装误差、MAX17048 电量曲线、TP4056 温升与负载共享、
GPIO 启动/下载复用，以及 BLE 手机/电脑断电重连都必须按
[`board_bringup_v2.md`](../../00_docs/testing/board_bringup_v2.md) 在真实样机验证。
