# Dependency Rules

## 1. Purpose

本文档定义项目的依赖规则。

这些规则属于架构约束，而不是编码建议。

任何新模块和代码修改都必须遵守这些规则。

---

## 2. Fundamental Rule

系统遵循依赖倒置原则：

> 高层业务逻辑不能依赖底层实现细节。

基本依赖关系：

```text
              07_products
                  │
                  ▼
              03_services
               │      │
               ▼      ▼
            01_core 02_ports
                      │
                      ▼
                   01_core
```

具体实现：

```text
05_adapters
     │
     ▼
02_ports
     │
     ▼
01_core
```

BSP 提供具体板级硬件信息：

```text
05_adapters
     │
     ▼
06_bsp
```

---

## 3. Core Rules

`01_core` 位于依赖图最内层。

允许：

```text
Core → Core
```

禁止：

```text
Core → Ports
Core → Services
Core → Protocol
Core → Adapters
Core → BSP
Core → Products
```

Core 禁止依赖：

```text
ESP-IDF
FreeRTOS
Zephyr
POSIX
具体硬件 Driver
```

Core 中禁止出现：

```cpp
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

Core 的 public API 禁止暴露：

```text
esp_err_t
gpio_num_t
TaskHandle_t
SemaphoreHandle_t
```

---

## 4. Port Rules

`02_ports` 定义系统需要的外部能力。

允许：

```text
Ports → Core
```

禁止：

```text
Ports → Services
Ports → Adapters
Ports → BSP
Ports → ESP-IDF
Ports → FreeRTOS
```

Port 必须描述能力，而不是实现。

例如：

正确：

```cpp
class ILightPort {
public:
    virtual Status set_brightness(float value) = 0;
};
```

错误：

```cpp
class ILightPort {
public:
    virtual esp_err_t set_ledc_duty(
        ledc_channel_t channel,
        uint32_t duty
    ) = 0;
};
```

后者泄漏了 ESP-IDF 实现细节。

---

## 5. Service Rules

`03_services` 负责业务规则和流程编排。

允许依赖：

```text
01_core
02_ports
04_protocol
```

禁止直接依赖：

```text
ESP-IDF
FreeRTOS
BSP
GPIO
PWM
LEDC
RMT
MCPWM
NVS
```

错误：

```cpp
void BehaviorService::happy()
{
    gpio_set_level(GPIO_NUM_8, 1);
}
```

正确：

```cpp
void BehaviorService::happy()
{
    motion_.play(MotionPattern::GentleSway);
    lighting_.play(LightPattern::SoftBreathing);
    haptic_.play(HapticPattern::DoubleSoftPulse);
}
```

---

## 6. Protocol Rules

`04_protocol` 描述通信数据结构和编解码规则。

允许依赖：

```text
01_core
```

必要时可以使用平台无关的公共类型。

Protocol 不允许依赖：

```text
BLE
Wi-Fi
ESP-IDF
FreeRTOS
Socket
UART Driver
```

协议与传输必须分离。

例如：

```text
CommandMessage
     ↓
Encoder
     ↓
byte[]
```

这些 byte 可以通过：

```text
BLE
Wi-Fi
UART
USB
```

中的任何一种方式传输。

---

## 7. Adapter Rules

`05_adapters` 是平台和硬件实现边界。

Adapter 可以依赖：

```text
02_ports
01_core
06_bsp
Vendor SDK
Operating System
```

Adapter 负责：

```text
Vendor API 调用
Vendor Error 转换
Callback 转换
Handle 生命周期
具体硬件控制
```

例如：

```text
esp_err_t
    ↓
EspBleAdapter
    ↓
Status
```

Vendor 类型不得穿透 Adapter 边界。

---

## 8. BSP Rules

`06_bsp` 描述：

> 当前是哪一块板，以及板上的硬件如何连接。

BSP 可以包含：

```text
GPIO Mapping
I2C Mapping
SPI Mapping
UART Mapping
Peripheral Presence
Board Revision
Hardware Limits
```

BSP 不包含：

```text
Behavior
BLE Protocol
Communication Policy
Motion Policy
Product Business Logic
```

例如：

```text
PLANT_V1_SERVO_PWM → GPIO 8
PLANT_V1_LED_DATA  → GPIO 4
```

属于 BSP。

而：

```text
Happy → Servo 运动
```

不属于 BSP。

---

## 9. Product Rules

`07_products` 是 Composition Root。

Product 可以知道具体实现。

例如：

```cpp
ServoAdapter motion(...);
LedAdapter light(...);
VibrationAdapter haptic(...);
EspBleAdapter ble(...);

BehaviorService behavior(
    motion_service,
    lighting_service,
    haptic_service
);
```

Product 负责：

```text
Dependency Injection
Feature Selection
BSP Selection
System Startup
```

Product 不应该重新实现通用业务逻辑。

---

## 10. State Ownership

每一种可变状态必须有且只有一个 Owner。

建议状态归属：

```text
DeviceState
    → LifecycleService

BehaviorState
    → BehaviorService

MotionState
    → MotionService

LightingState
    → LightingService

HapticState
    → HapticService

CommunicationState
    → CommunicationService
```

其他模块可以：

```text
查询状态
发送 Command
接收 Event
```

但不得直接修改状态。

禁止出现：

```text
BLE 模块修改 motion_state

Behavior 模块修改 ble_connected

LED 模块修改 device_state
```

---

## 11. Behavior / Hardware Boundary

Behavior 只能描述产品语义。

例如：

```text
Happy
Grow
Sleep
WakeUp
Attention
Calm
```

Behavior 不允许包含：

```text
GPIO
PWM Duty
Servo Angle
Stepper Count
LEDC Channel
```

正确：

```text
Behavior::Grow
     ↓
MotionPattern::Grow
```

错误：

```text
Behavior::Grow
     ↓
Servo = 62°
```

Servo 角度属于 Motion 实现或配置。

---

## 12. Communication Rules

模块间通信分为两类。

### Direct Call

当：

```text
调用关系明确
只有一个目标
需要明确返回值
属于同步操作
```

使用普通接口调用。

例如：

```cpp
storage.save(config);
```

禁止为了“解耦”改成：

```text
SaveConfigEvent
```

---

### Event

当：

```text
真正异步
跨线程
硬件 Callback
广播状态变化
```

才使用 Event。

例如：

```text
BleConnected
BleDisconnected
MotionCompleted
ButtonPressed
LowBatteryDetected
```

---

## 13. Callback Boundary

Vendor Callback 不允许直接执行业务逻辑。

错误：

```text
ESP BLE Callback
      ↓
BehaviorService
      ↓
Servo
```

正确：

```text
ESP BLE Callback
      ↓
EspBleAdapter
      ↓
Event / Queue
      ↓
CommunicationService
      ↓
Command
      ↓
BehaviorService
```

---

## 14. Error Boundary

具体平台 Error 必须在 Adapter 内转换。

例如：

```text
ESP_ERR_TIMEOUT
      ↓
Adapter
      ↓
Status::Timeout
```

业务层不得判断：

```cpp
if (err == ESP_ERR_TIMEOUT)
```

而应该判断：

```cpp
if (status == Status::Timeout)
```

---

## 15. Dependency Summary

允许的主要依赖：

```text
07_products
    ├── 03_services
    ├── 05_adapters
    └── 06_bsp

03_services
    ├── 01_core
    ├── 02_ports
    └── 04_protocol

04_protocol
    └── 01_core

05_adapters
    ├── 01_core
    ├── 02_ports
    └── 06_bsp

02_ports
    └── 01_core

01_core
    └── nothing
```

架构的核心原则是：

> **Dependency points toward policy, not implementation.**

即：

> **依赖指向策略，而不是指向实现细节。**

