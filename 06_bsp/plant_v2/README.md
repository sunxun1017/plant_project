<!--
 * @Author: sunxun sx2728977548@163.com
 * @Date: 2026-08-22 14:15:18
 * @LastEditors: sunxun sx2728977548.com
 * @LastEditTime: 2026-08-22 15:26:20
 * @FilePath: /plant_project/06_bsp/plant_v2/README.md
 * @Description:
-->
# Plant V2 BSP

`plant_v2_board.hpp` 是 V2 板级身份、GPIO、ADC 通道、电气极性、安全范围和默认标定值
的唯一来源。当前数值是交叉编译基线，不是上板结论。

当前头文件还临时承载了一部分产品策略默认值；这不代表这些策略属于 BSP。哪些字段应
迁移到产品配置、Protocol 或 sdkconfig，见
[`v2_bsp_boundary_review.md`](../../00_docs/architecture/v2_bsp_boundary_review.md)。后续迁移应
保持行为不变，并由 V1/V2 双目标构建和主机快照测试保护。

原理图或样机数据变化时只修改该文件，并同步执行
[`board_bringup_v2.md`](../../00_docs/testing/board_bringup_v2.md)。GPIO18/19 固定保留给原生
USB Serial/JTAG；AHT21/MAX17048 共用的 I²C 改用 GPIO2/9。两根总线的上拉必须满足正常
启动电平，GPIO9 上的 BOOT 按键仍须可靠进入下载模式。样机还要确认三路 ADC 的端点、
噪声和串扰。

当前产品变体固定为普通共阴 5050 RGB 三路 PWM，不是 WS2812B。RGB 三色与 0827 马达
都必须通过外部低边驱动，不能直接由 ESP32-C3 GPIO 承担负载电流；马达还要有续流/钳位
保护。GPIO20/21 与 UART0 下载/日志复用，下载夹具、TTP223 输出和马达驱动之间必须有
可验证的隔离方案。

TP4056 只负责单节锂电充电。V2 原理图还必须提供受保护电芯（或独立保护电路）、充电时
负载共享、稳定的 3.3 V 传感器电源和能承受舵机峰值电流的独立功率路径。MAX17048 与
AHT21 共用 I²C，用于电量/电压遥测；TP4056 的 `CHRG`/`STDBY` 未接 MCU，所以固件不会
伪造“正在充电”状态。
