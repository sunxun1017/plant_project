# ZW V1.0 联调固件

独立板级联调入口，**尚未接入原有 BLE/OTA/生长业务，不是量产应用**。新配置默认选择 `zw_v1`；原 `v2` 目标保留，可显式选择，仅适用于旧假设硬件。

先阅读 [硬件审查](../../00_docs/hardware/zw_v1_review.md)。默认固件会输出硬件检查提示，然后停止初始化，不使能电源支路。修正 PNP 基极限流、核对电源域后，才能打开 `ZW_BOARD_POWER_PATH_VERIFIED`。

## 构建

使用 ESP-IDF 6.1 和 ESP32-C3 工具链。在已激活的 IDF 环境中，于仓库根目录执行：

```powershell
idf.py -B build-zw "-DPLANT_PRODUCT=zw_v1" "-DSDKCONFIG=sdkconfig.zw_v1" build
idf.py -B build-zw "-DPLANT_PRODUCT=zw_v1" "-DSDKCONFIG=sdkconfig.zw_v1" menuconfig
```

PowerShell 中带点号的 `-D` 参数必须整体引用，避免拆成额外 Ninja target。Windows 仓库路径过长时，使用短的绝对构建路径，例如 `-B C:/work/zw-build`；IDF 会解析 subst 的实际路径，仅做盘符映射不一定够。

在 menuconfig 的 `ZW V1.0 board bring-up` 中：

- `ZW_BOARD_POWER_PATH_VERIFIED`：默认关闭。仅在实际硬件确认后开启。
- `ZW_TEST_AUDIO`：默认关闭。启用时测试一次 ES8311 PCM 音量，完成后回到软件 standby。
- `ZW_TEST_DEEP_SLEEP`：默认关闭。开启后启动 30 秒进入深睡，60 秒定时或触摸中断唤醒。USB 会断开。

所有配置都编译 ES8311 驱动及板级静态检查。无论是否打开音频采样，启用硬件联调后都会先配置 ES8311 standby；若无法确认 standby 则停止进一步测试。VDD33 和音频数字电源保持开启，避免对仍有 I²C/模拟电源的设备局部断电。

充电使能、舵机动作、振动、雷达业务不在启动流程中自动执行。BQ24259 仅报告原始状态及故障，不提供虚构的电压/电量。

## 烧录和调试

本次没有烧录任何设备。经硬件检查后，可在同一构建配置下手动执行：

```powershell
idf.py -B build-zw "-DPLANT_PRODUCT=zw_v1" "-DSDKCONFIG=sdkconfig.zw_v1" -p COM端口 flash monitor
```

这是独立联调分区配置，不是旧 `v2` 的 OTA 升级镜像；不要通过旧版 OTA 发送。需保留旧机数据时先备份 flash。使用 USB Serial/JTAG，应用禁止改用 UART0 控制台，因为 GPIO20/21 已连接音频 MCLK/舵机 PWM。

## 代码入口

- `06_bsp/zw_v1/board.hpp`：已确认引脚、地址、PCF8574 位图。
- `05_adapters/espidf/zw_v1/hardware.*`：共享 I²C、PCF8574 输出影子、触摸、温湿度、充电状态、ADC 和入睡准备。
- `05_adapters/espidf/zw_v1/audio.*`：官方 codec 组件和有超时限制的 PCM 采样。
- `08_tests/unit/zw_v1_board_test.cpp`：输出隔离、P0/P5 约束、CRC 损坏及量程端点静态检查。
- `09_tools/hardware_tools/extract_pcb_nets.py`：可重现 PCB 焊盘网络提取；需 `olefile`，不运行附件中的代码。

PCF8574 只能由一个任务持有并更新，不可在 ISR 中做 I²C。传输失败后锁存影子标为未知，禁止继续使能执行器；只有完整写入确定状态后恢复。

依赖固定为 `esp_codec_dev 1.5.3`。IDF 6.1 构建时，其 SPI 源文件缺少 `esp_idf_version.h` 的显式包含，顶层 CMake 对该组件单独加预包含；没有修改下载缓存或官方组件源码。
