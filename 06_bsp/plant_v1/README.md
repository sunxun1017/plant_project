# Plant V1 BSP

Plant V1 当前统一按 ESP32-C3 配置。

所有临时 GPIO、PWM、舵机脉宽、触摸阈值、低功耗参数和产品标识集中在
[`plant_v1_board.hpp`](plant_v1_board.hpp)。ESP-IDF Adapter 只能引用这些配置，不能另外定义板级常量。

当前值是首轮开发默认值，接线或机械结构确定后允许整体修改。修改舵机参数时必须保持编译期安全断言通过，并重新执行主机测试与上板安全行程测试。
