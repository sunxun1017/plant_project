# Plant V2 BSP 与配置边界审查

## 1. 结论

Plant V2 的 BSP 不是“内容太少”。传感器本身当然属于板级支持：型号和存在性、引脚、
地址、电路、电气范围与器件采样时序都必须由 BSP 描述。此前真正的问题是职责过宽：
板级事实、产品语义、协议身份和运行时默认值被放进了同一个 `BoardConfig`；本轮已将
产品语义迁至 `10_config/plant_v2/plant_v2_product_config.hpp`。

BSP 文件数量少本身没有问题。一块板使用一个集中式、只含编译期常量的头文件，反而容易
审查和替换。判断边界是否正确，应看它回答的是不是下面这个问题：

> 这块板上有什么，以及它如何连接和受到什么物理限制？

如果一个值回答的是“产品什么时候睡眠”“讲话多久算持续”“BLE 广播多快”或“互动多久
增长一次”，它就不应该因为使用方便而放进 BSP。

## 2. 当前做得正确的部分

以下内容属于 BSP：

- ESP32-C3 GPIO、ADC 通道、I²C 引脚和外设地址；
- GPIO 有效电平、内部上下拉和外部驱动要求；
- 舵机 PWM 频率、脉宽硬限制和机械安全位置；
- ADC 电气有效范围、电位器方向和近电源轨故障边界；
- GL5528 分压电阻、电容和读数方向；
- AHT21/MAX17048 的板载存在性、地址和总线能力；
- 电池电压、容量范围、受保护电芯和负载共享要求；
- LED、马达驱动拓扑和最大连续硬件限制；
- GPIO 唯一性、USB/Flash 保留脚和安全范围的 `static_assert`。

本轮把 I²C 从 GPIO18/19 移到 GPIO2/9，并增加引脚冲突检查，是典型的 BSP 修复。

## 3. 已迁出 BSP 的产品语义

| 当前分组 | 不应由 BSP 拥有的值 | 更合适的 Owner |
| --- | --- | --- |
| `Microphone` | 讲话开始/结束时间、持续讲话窗口和累计时间 | Product defaults → `AcousticService` |
| `Illumination` | Dark/Bright 语义门限、确认/退出时间、促进时间片 | Product defaults → `IlluminationService` |
| `Climate` | “适宜”温湿度区间、迟滞和促进时间片 | Product defaults → `ClimateService` |
| `Battery` | Low/Critical 产品门限 | Product defaults → `BatteryService` |
| `Touch` | 防抖、最小时长 | Product interaction defaults |
| `Growth` | 生长/回落步长、队列容量、无互动门限和回落周期 | Product defaults → `GrowthService` |
| `Interaction` | 自动休眠和 System Tick | Product composition/runtime config |
| `Power` | 轻睡眠调度与电源锁策略 | Product power policy |
| `Product` | 设备名、产品 ID、固件版本、OTA 分片大小 | Product/build/protocol config |
| `Ble` | UUID、MTU、队列深度、广播策略、绑定/连接策略 | Protocol + product transport config |

有些分组同时包含物理事实和产品策略，不能整组搬走。例如：

- `Microphone::valid_raw_*` 是电气/标定事实，属于 BSP；讲话门限和时间属于 Service 配置。
- `Illumination::fixed_resistor_ohm` 属于 BSP；`bright_confirm_ms` 属于产品策略。
- `Climate::address` 属于 BSP；`suitable_min_temperature` 属于产品定义。
- `Touch::active_high` 和内部下拉属于 BSP；防抖时间与抚摸最小时长属于产品交互配置。
- CPU 可用频率受芯片和板级电源约束；是否在某状态进入深睡眠属于产品策略。

## 4. 仍需继续控制的漂移风险

### 4.1 构建配置与 BSP 重复

`BoardConfig::Ble::maximum_bonds` 和 `maximum_connections` 是声明值，但实际 NimBLE 容量由
`sdkconfig.defaults` 的 `CONFIG_BT_NIMBLE_MAX_BONDS` 与
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS` 决定。两份值没有编译期关联，后续修改一边可能造成
文档、遥测或产品预期与实际控制器容量不一致。

### 4.2 通用 Adapter 默认依赖 V1 BSP（已修复）

BLE、触摸、电源、RGB 和振动 Adapter 曾因默认构造函数直接依赖
`bsp::v1::BoardConfig`。本轮已删除这些默认依赖，V1/V2 组合根均显式构造窄配置，避免：

- 单独复用 Adapter 时静默落回 V1 GPIO 或策略；
- clangd 和依赖审查误以为通用 Adapter 必须依赖 V1；
- 新产品忘记显式注入时，编译可以通过但硬件行为错误。

舵机 Adapter 分别是明确的 V1/V2 板型实现，仍可直接依赖对应 BSP；它们不是跨板通用
Adapter。

### 4.3 缺少独立产品配置层（已修复）

`10_config/plant_v2/plant_v2_product_config.hpp` 现已承接传感识别门限、成长节奏、交互、
低功耗、产品身份和 BLE 运行策略。BSP 继续完整拥有传感器硬件支持。

## 5. 推荐的目标结构

```text
01_core/domain/configuration.hpp
    平台无关配置类型、单位和范围不变量

06_bsp/plant_v2/plant_v2_board.hpp
    BoardIdentity
    PinMap / PeripheralPresence
    ElectricalPolarity
    AdcElectricalLimits
    MechanicalHardLimits
    PowerTopology

10_config/plant_v2/plant_v2_product_config.hpp
    AcousticDetectionConfig
    IlluminationDetectionConfig
    ClimateDetectionConfig
    GrowthConfig
    InteractionConfig
    LowPowerConfig
    BleRuntimeConfig

sdkconfig.defaults
    NimBLE/FreeRTOS/PM/OTA 等必须在编译期确定的供应商选项

07_products/plant_v2/composition_root.cpp
    选择 V2 BSP 和 defaults
    把窄配置显式注入 Adapter 与 Service
```

Protocol UUID 和版本也可以放在 `04_protocol` 的权威定义中，由 V1/V2 协议各自拥有，避免
BSP 和工具重复维护。

## 6. 本轮迁移结果与后续步骤

已完成产品配置文件、通用 Adapter 显式注入、组合根分离和 `BoardConfig` 策略字段清理；
主机测试与 V1/V2 ESP-IDF 构建负责保护边界。后续仍要：

1. 把 UUID/协议版本进一步收敛到 Protocol 权威定义。
2. 把 NimBLE 容量等编译期值与 `sdkconfig.defaults` 建立一致性检查。
3. 最终样机标定后，只在 BSP 修改电气/器件事实，只在 ProductConfig 修改体验语义。

## 7. 审查准则

以后新增常量时先回答：

- 换一块 PCB 但产品体验不变，这个值会变吗？会变则倾向 BSP。
- 同一块 PCB 换产品策略，这个值会变吗？会变则不属于 BSP。
- 这是供应商栈编译容量吗？放到 sdkconfig/Kconfig。
- 这是线上可校准值吗？使用 Core 配置类型和持久化策略，不要写死在 BSP。
- 这是协议身份或兼容规则吗？由 Protocol/Product 拥有。

因此，BSP 可以很小，但必须足够完整地描述硬件；不能为了让 BSP“看起来丰富”而把产品
行为塞进去。当前 V2 的方向应是拆出策略，而不是继续扩大 `BoardConfig`。
