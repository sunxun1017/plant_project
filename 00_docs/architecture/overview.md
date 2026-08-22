# System Architecture Overview

## 1. Purpose

本项目实现一个小型具身交互植物设备。

设备可以通过机械运动、灯光、振动等方式表达自身状态，并通过 Bluetooth Low Energy 与手机进行双向通信。未来系统可以继续增加 Wi-Fi、环境传感器、声音、触摸、电池管理等能力。

本项目的软件架构目标是：

* 平台无关
* 高内聚、低耦合
* 硬件可替换
* 操作系统可替换
* 业务逻辑可在 PC 上测试
* 各模块职责清晰
* 支持产品持续迭代
* 避免业务代码直接依赖 ESP-IDF、FreeRTOS 或具体 GPIO

本项目最核心的产品原则是：

> **Behavior is the product; hardware is the actuator.**

即：

> **行为是产品，硬件只是行为的执行器。**

用户感知到的是植物“抬头、摇摆、呼吸、轻轻振动”等行为，而不是 PWM、GPIO、LEDC Channel 或具体电机型号。

---

## 2. Architecture Style

系统采用 Ports and Adapters Architecture。

核心业务逻辑不直接依赖具体硬件和操作系统。

整体结构：

```text
                    Mobile App
                        │
                   BLE / Wi-Fi
                        │
                        ▼
              Communication Service
                        │
                        ▼
                 Command / Event
                        │
                        ▼
                Behavior Service
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
       Motion        Lighting       Haptic
       Service        Service       Service
          │             │             │
          ▼             ▼             ▼
     IMotionPort     ILightPort    IHapticPort
          │             │             │
          ▼             ▼             ▼
      Adapters / Hardware Drivers
          │             │             │
          ▼             ▼             ▼
 Servo / Stepper      LED         Vibration
```

系统另外具有 Device Lifecycle，用于维护设备整体运行状态：

```text
Booting
   ↓
Idle
   ↓
Interacting
   ↓
Sleeping

同时可能进入：

LowBattery
Fault
Updating
```

---

## 3. Architectural Layers

### 3.1 Core

`01_core` 是整个系统最稳定的部分。

它包含：

* Device State
* Behavior
* Command
* Event
* Configuration
* Result
* Status
* Error

Core 不知道：

* ESP32
* ESP-IDF
* FreeRTOS
* BLE Stack
* GPIO
* PWM
* Servo
* Stepper

Core 必须能够在普通 Linux PC 上独立编译和测试。

---

### 3.2 Ports

`02_ports` 定义系统需要的外部能力。

Port 描述：

> 系统需要什么。

而不是：

> ESP32 如何实现。

主要 Ports：

```text
IMotionPort
ILightPort
IHapticPort
IBlePort
IWifiPort
IStoragePort
IClockPort
ILoggerPort
IExecutorPort
ITimerPort
```

Port 中禁止出现具体平台类型。

例如禁止：

```cpp
esp_err_t
gpio_num_t
TaskHandle_t
SemaphoreHandle_t
```

---

### 3.3 Services

`03_services` 实现产品行为和业务流程。

主要 Service：

```text
BehaviorService
MotionService
LightingService
HapticService
CommunicationService
LifecycleService
DiagnosticsService
```

Service 可以依赖 Core 和 Ports。

Service 不允许直接调用：

```text
ESP-IDF
FreeRTOS
GPIO
PWM
LEDC
RMT
MCPWM
NVS
```

---

## 4. Behavior Architecture

Behavior 是产品交互体验的核心。

Behavior 使用语义而不是硬件参数表达行为。

例如：

```text
Happy
WakeUp
Sleep
Grow
Excited
Attention
Calm
LowBattery
Error
```

一个 Behavior 可以同时驱动多个表现通道。

例如：

```text
Behavior::Happy

Motion
    → GentleSway

Lighting
    → SoftBreathing

Haptic
    → DoubleSoftPulse
```

Behavior 不知道：

```text
舵机角度
步进电机步数
PWM Duty
GPIO
振动马达电压
LED Channel
```

Behavior 只描述产品语义。

---

## 5. Motion Architecture

Motion 模块管理植物的机械运动。

上层使用：

```text
Grow
Retract
Sway
Wake
Sleep
MoveToPose
Stop
```

而不是：

```text
Servo = 37°
Stepper = 840 steps
PWM = 1500 us
```

转换关系：

```text
Behavior
   ↓
Motion Semantic
   ↓
MotionService
   ↓
Motion Profile / Trajectory
   ↓
IMotionPort
   ↓
ServoAdapter / StepperAdapter
   ↓
Hardware
```

机械结构变化只能影响 Motion Adapter、BSP 或配置参数，不应该影响 Behavior。

---

## 6. Lighting Architecture

Lighting 模块负责所有视觉反馈。

典型语义：

```text
Off
Solid
Breathing
Blink
Pulse
Fade
```

例如：

```text
LowBattery
   ↓
SlowRedBreathing
```

Lighting 动画必须是非阻塞的。

禁止：

```cpp
for (...) {
    set_brightness(...);
    delay(...);
}
```

动画应由 Timer、Animator 或调度机制推进。

---

## 7. Haptic Architecture

Haptic 模块负责振动反馈。

上层描述振动 Pattern：

```text
SoftPulse
DoublePulse
LongPulse
Heartbeat
Warning
```

Behavior 不控制具体 PWM 和持续时间实现。

具体执行方式由 IHapticPort 的 Adapter 决定。

---

## 7.1 V2 Sensing Architecture

V2 增加位置、声学活动、相对光照和温湿度感知。传感器输出先转换为平台无关的语义
状态，再进入产品策略：

```text
Position Potentiometer → Position Adapter ───────────┐
ECM Envelope ─────────→ Acoustic Activity Service ──┤
GL5528 Divider ────────→ Light Exposure Service ─────┼→ Growth Service
AHT20 ─────────────────→ Climate Service ────────────┤       │
Touch ─────────────────→ Touch Service ──────────────┘       ▼
                                                       Motion / Lighting
```

四类输入只产生带来源和时间戳的 `GrowthCredit`。Growth Service 统一处理冷却、限幅、
过期和执行器占用；Sensor Adapter 不直接控制舵机或灯光。

ECM 链路只向上层提供包络音量和讲话活动，不保存、传输或持久化原始音频。GL5528 只
表示相对明暗，AHT20 通过非阻塞 I²C 状态机读取温湿度。

---

## 8. Communication Architecture

手机通信属于 Communication Service。

BLE 和 Wi-Fi 是传输机制，不是业务逻辑。

正确的数据流：

```text
Phone
  ↓
BLE
  ↓
BleAdapter
  ↓
CommunicationService
  ↓
Protocol Decoder
  ↓
Command
  ↓
Behavior / Configuration / Lifecycle
```

禁止：

```text
BLE Callback
   ↓
直接控制 Servo
```

也禁止：

```text
BLE Callback
   ↓
直接控制 LED
```

BLE 只负责通信。

---

## 9. Protocol Architecture

Protocol 描述设备与手机之间的数据语义。

例如：

```text
SetBehavior
SetConfig
GetState
GetTelemetry
Ping
```

Protocol 层负责：

```text
Message
Encoding
Decoding
Version
Validation
```

Protocol 层不负责：

```text
BLE Connection
Wi-Fi Socket
UART
具体 Transport
```

因此同一套协议未来可以运行在：

```text
BLE
Wi-Fi
USB
UART
```

之上。

---

## 10. State Ownership

每个状态必须只有一个明确 Owner。

```text
Device State
    → LifecycleService

Behavior State
    → BehaviorService

Motion State
    → MotionService

Lighting State
    → LightingService

Haptic State
    → HapticService

BLE Connection State
    → CommunicationService
```

其他模块不得直接修改不属于自己的状态。

模块之间通过：

```text
Command
Event
Public Interface
```

进行协作。

---

## 11. Hardware Isolation

硬件实现位于：

```text
05_adapters
06_bsp
```

例如：

```text
MotionService
     ↓
IMotionPort
     ↓
ServoAdapter
     ↓
ESP-IDF LEDC/MCPWM
     ↓
GPIO
```

如果：

```text
Plant V1 → Servo

Plant V2 → Servo + Position Potentiometer
```

理想情况下：

```text
BehaviorService
LightingService
CommunicationService
Protocol
```

都不应该修改。

---

## 12. Product Composition

`07_products` 是系统的 Composition Root。

它负责选择：

```text
哪一个 BSP
哪一个 Motion Adapter
哪一个 BLE Adapter
使用哪些 Services
使用哪些 Feature
```

例如：

```text
Plant V1

ESP32-C3
+ Servo
+ RGB LED
+ Vibration Motor
+ BLE
+ Wi-Fi
```

V2：

```text
Plant V2

ESP32-C3
+ Servo + Position Potentiometer
+ RGB LED
+ Vibration Motor
+ BLE
+ Touch Sensor
+ ECM Envelope Sensor
+ GL5528 Light Sensor
+ AHT20 Temperature/Humidity Sensor
```

核心业务模块仍然可以复用。

---

## 13. Design Goal

最终希望做到：

```text
                   Product Behavior
                         │
                         ▼
                     Services
                         │
                         ▼
                       Ports
                         │
              ┌──────────┼───────────┐
              ▼          ▼           ▼
            ESP-IDF    Linux       Fake
              │                      │
              ▼                      ▼
           Hardware               PC Tests
```

系统真正稳定的是产品语义和业务规则。

芯片、OS、通信方式和执行器都是可以替换的实现细节。
