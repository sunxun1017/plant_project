# Plant V2 BSP 与配置边界审查

## 1. 结论

Plant V2 的 BSP 不是“内容太少”。当前 `plant_v2_board.hpp` 已有约 310 行，真正的问题是
职责过宽：板级连接、电气限制、产品策略、协议身份和运行时默认值被放进了同一个
`BoardConfig`。

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

## 3. 当前越过 BSP 边界的内容

| 当前分组 | 不应由 BSP 拥有的值 | 更合适的 Owner |
| --- | --- | --- |
| `Microphone` | 讲话开始/结束时间、持续讲话窗口和累计时间 | Product defaults → `AcousticService` |
| `Illumination` | Dark/Bright 语义门限、确认/退出时间、促进时间片 | Product defaults → `IlluminationService` |
| `Climate` | “适宜”温湿度区间、迟滞和促进时间片 | Product defaults → `ClimateService` |
| `Battery` | Low/Critical 产品门限 | Product defaults → `BatteryService` |
| `Touch` | 防抖、2 秒长按、10 秒恢复绑定 | Product interaction defaults |
| `Growth` | 步长、四类冷却、待处理过期时间 | Product defaults → `GrowthService` |
| `Interaction` | 自动休眠和 System Tick | Product composition/runtime config |
| `Power` | 是否启用深睡眠、进入深睡眠等待时间、定时唤醒周期 | Product power policy |
| `Product` | 设备名、产品 ID、固件版本、OTA 分片大小 | Product/build/protocol config |
| `Ble` | UUID、MTU、队列深度、广播策略、绑定/连接策略 | Protocol + product transport config |

有些分组同时包含物理事实和产品策略，不能整组搬走。例如：

- `Microphone::valid_raw_*` 是电气/标定事实，属于 BSP；讲话门限和时间属于 Service 配置。
- `Illumination::fixed_resistor_ohm` 属于 BSP；`bright_confirm_ms` 属于产品策略。
- `Climate::address` 属于 BSP；`suitable_min_temperature` 属于产品定义。
- `Touch::active_high` 和内部下拉属于 BSP；长按语义属于产品交互配置。
- CPU 可用频率受芯片和板级电源约束；是否在某状态进入深睡眠属于产品策略。

## 4. 已发现的具体漂移风险

### 4.1 构建配置与 BSP 重复

`BoardConfig::Ble::maximum_bonds` 和 `maximum_connections` 是声明值，但实际 NimBLE 容量由
`sdkconfig.defaults` 的 `CONFIG_BT_NIMBLE_MAX_BONDS` 与
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS` 决定。两份值没有编译期关联，后续修改一边可能造成
文档、遥测或产品预期与实际控制器容量不一致。

### 4.2 通用 Adapter 默认依赖 V1 BSP

多个 ESP-IDF Adapter 为提供默认构造函数，直接 include 或 alias
`bsp::v1::BoardConfig`。V2 组合根虽然显式传入了配置，但通用 Adapter 本身仍带有 V1
产品依赖，容易产生以下问题：

- 单独复用 Adapter 时静默落回 V1 GPIO 或策略；
- clangd 和依赖审查误以为通用 Adapter 必须依赖 V1；
- 新产品忘记显式注入时，编译可以通过但硬件行为错误。

目标状态应是：通用 Adapter 只接受自己的窄配置结构；V1/V2 组合根负责从各自 BSP 和产品
配置构造这些结构。若要保留便利默认值，也应由 V1 产品包装器提供，而不是放在通用 Adapter。

### 4.3 缺少独立产品配置层

仓库已有 `10_config` 的架构约定，但当前没有实际配置文件。产品默认值因此自然堆进
`BoardConfig`。这不是 BSP 缺文件，而是配置 Owner 尚未落地。

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

10_config/plant_v2/product_defaults.hpp
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

## 6. 建议迁移步骤

这项重构不应该和硬件功能修复混在一个提交中。建议分阶段：

1. 新建平台无关的 V2 产品默认配置，只移动值，不改行为。
2. 为每个 Adapter 保留窄配置结构，删除对 V1 BSP 的默认依赖。
3. 组合根显式完成 BSP → Adapter、Product defaults → Service 的注入。
4. 把 NimBLE 容量等编译期值集中到 `sdkconfig.defaults`，增加 CMake/Kconfig 一致性检查。
5. 主机快照测试确认迁移前后策略值一致，V1/V2 分别做新鲜 ESP-IDF 构建。
6. 最后删除 `BoardConfig` 中已迁走的策略字段，并更新本文件。

## 7. 审查准则

以后新增常量时先回答：

- 换一块 PCB 但产品体验不变，这个值会变吗？会变则倾向 BSP。
- 同一块 PCB 换产品策略，这个值会变吗？会变则不属于 BSP。
- 这是供应商栈编译容量吗？放到 sdkconfig/Kconfig。
- 这是线上可校准值吗？使用 Core 配置类型和持久化策略，不要写死在 BSP。
- 这是协议身份或兼容规则吗？由 Protocol/Product 拥有。

因此，BSP 可以很小，但必须足够完整地描述硬件；不能为了让 BSP“看起来丰富”而把产品
行为塞进去。当前 V2 的方向应是拆出策略，而不是继续扩大 `BoardConfig`。
