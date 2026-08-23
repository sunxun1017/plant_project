# Threading Model

## 1. Purpose

本文档定义系统的执行上下文、Task、ISR、Timer、Callback 和异步事件处理原则。

目标是避免：

```text
一个模块一个 Task
一个功能一个线程
大量 Mutex
大量共享状态
Callback 中执行复杂业务
```

系统优先采用：

> **少量长期 Task + Event Queue + State Machine + Hardware Peripheral**

---

## 2. Fundamental Principle

> **Thread is an execution resource, not a module boundary.**

即：

> **线程是一种执行资源，不是模块边界。**

存在：

```text
MotionService
LightingService
BehaviorService
HapticService
```

并不意味着必须存在：

```text
motion_task
lighting_task
behavior_task
haptic_task
```

Service 默认不拥有线程。

执行资源由系统统一提供。

---

## 3. Initial Runtime Model

第一版本建议保持简单：

```text
                Hardware ISR
                     │
                     ▼
                Event Queue
                     │
                     │
ESP-IDF Callback ────┤
                     │
                     ▼
              ┌──────────────┐
              │ System Task  │
              └──────┬───────┘
                     │
       ┌─────────────┼──────────────┐
       ▼             ▼              ▼
   Behavior       Lifecycle      Commands
       │
       ├─────────────┬──────────────┐
       ▼             ▼              ▼
    Motion        Lighting        Haptic
       │             │              │
       ▼             ▼              ▼
    Adapter        Adapter        Adapter
```

通信可以根据复杂度拥有独立 IO Task：

```text
BLE / Wi-Fi Callback
        │
        ▼
   Communication
      Queue
        │
        ▼
Communication Task
        │
        ▼
Protocol Decoder
        │
        ▼
Command Queue
        │
        ▼
System Task
```

第一版本不需要为了理论上的并行能力创建大量 Task。

---

## 4. System Task

System Task 负责主要业务状态推进。

包括：

```text
Device Lifecycle
Behavior State Machine
Command Processing
High-Level Motion Command
Lighting Command
Haptic Command
```

System Task 可以调用非阻塞 Service API。

System Task 不应该：

```text
等待长时间网络 IO
执行 delay 动画
生成精确步进脉冲
执行长时间 Flash 操作
等待 BLE 数据
```

---

## 5. Communication Execution

BLE/Wi-Fi Vendor Callback 只负责：

```text
复制必要数据
转换事件
投递 Queue
快速返回
```

禁止：

```text
BLE Callback
   ↓
复杂 JSON 解析
   ↓
Behavior
   ↓
控制电机
   ↓
等待运动完成
```

正确：

```text
BLE Callback
   ↓
Adapter
   ↓
RX Queue
   ↓
Communication Task
   ↓
Decode
   ↓
Command
   ↓
System Queue
```

---

## 6. ISR Rules

ISR 只执行必要的实时操作。

允许：

```text
读取硬件状态
记录 timestamp
清除 interrupt flag
向 ISR-safe Queue 投递事件
```

禁止：

```text
日志格式化
动态内存分配
协议解析
Behavior 执行
BLE 发送
Flash 写入
复杂状态机
长时间计算
```

ISR 原则：

> **Capture, signal, return.**

即：

> **捕获信息、发出通知、立即返回。**

---

## 7. Motion Execution

Motion 分为两层：

```text
High-Level Motion
      ↓
MotionService
      ↓
Motion Profile / Trajectory
      ↓
Motion Adapter
      ↓
Hardware Timing
```

System Task 不负责精确电机时序。

例如步进电机：

错误：

```cpp
while (steps--) {
    gpio_set_level(step_pin, 1);
    delay_us(...);
    gpio_set_level(step_pin, 0);
    delay_us(...);
}
```

这种方式会占用业务执行上下文。

应该优先：

```text
Hardware Timer
RMT
MCPWM
Dedicated Peripheral
Motor Driver IC
```

实现精确时序。

业务层只发送：

```text
MoveTo
PlayPattern
Stop
Home
```

---

## 8. Motion Completion

运动结束应产生异步事件：

```text
MotionService
    ↓
IMotionPort
    ↓
Motor Adapter
    ↓
Hardware
    ↓
MotionCompleted
    ↓
Event Queue
    ↓
System Task
```

Behavior 可以等待逻辑上的 MotionCompleted，

但不能通过阻塞线程：

```cpp
while (!motor_finished) {}
```

进行等待。

---

## 9. Lighting Execution

Lighting animation 必须非阻塞。

禁止：

```cpp
for (int i = 0; i < 100; ++i) {
    set_brightness(i);
    delay(10);
}
```

推荐模型：

```text
LightCue::Happy
        ↓
LightingService
        ↓
Light Animator
        ↓
Periodic Tick / Hardware PWM
        ↓
ILightPort
```

业务层只描述：

```text
Breathing
Blink
Fade
Solid
Off
```

而不负责逐帧更新。

---

## 10. Haptic Execution

振动 Pattern 同样采用非阻塞方式。

例如：

```text
DoublePulse
```

不应该：

```text
ON
delay
OFF
delay
ON
delay
OFF
```

阻塞 System Task。

而应该：

```text
HapticPattern
      ↓
HapticService
      ↓
Timer / Pattern State Machine
      ↓
IHapticPort
```

---

## 11. Timer Rules

Timer 主要用于：

```text
Behavior timeout
LED animation tick
Haptic pattern
Retry timer
Debounce
Periodic telemetry
```

Timer Callback 应保持轻量。

复杂逻辑通过：

```text
Timer
 ↓
Event
 ↓
System Task
```

完成。

---

## 12. Blocking Rules

System Task 中默认禁止长时间 blocking。

以下操作应谨慎：

```text
Network IO
Flash erase/write
Motor wait
Sleep/delay
Blocking queue with unlimited timeout
```

允许短时间、边界明确的同步操作。

所有可能持续较长时间的操作必须明确设计其异步行为。

---

## 13. Shared State

优先避免共享可变状态。

正确模型：

```text
State Owner
    │
    ├── Query API
    └── Command API
```

而不是：

```text
global_device_state
global_motor_state
global_ble_state
```

禁止多个 Task 直接读写同一个业务状态变量。

---

## 14. Synchronization

Mutex 不是默认设计手段。

优先级：

```text
1. Single ownership
2. Message passing
3. Immutable data
4. Queue
5. Mutex
```

只有真正存在共享资源时才使用 Mutex。

不能因为“以后可能多线程”就提前给所有类加锁。

---

## 15. Recommended Initial Tasks

第一阶段推荐最多保持：

```text
System Task

Communication Task
```

再加 ESP-IDF 本身内部需要的系统任务。

必要时后续增加：

```text
Storage Worker
OTA Worker
```

但必须有真实需求后再增加。

不要从第一天创建：

```text
Behavior Task
Motion Task
LED Task
Haptic Task
State Task
Config Task
Logging Task
```

---

## 16. Execution Overview

推荐的数据流：

```text
                  ┌───────────────┐
                  │ Hardware ISR  │
                  └───────┬───────┘
                          │
                          ▼
                       Queue
                          │
                          ▼
Phone → BLE Adapter → Communication Task
                          │
                          ▼
                       Command
                          │
                          ▼
                     System Task
                          │
                    Behavior FSM
                          │
             ┌────────────┼─────────────┐
             ▼            ▼             ▼
          Motion       Lighting       Haptic
             │            │             │
             ▼            ▼             ▼
         Hardware      Animator      Pattern FSM
```

系统应尽可能通过：

```text
Command
Event
State Machine
Hardware Peripheral
```

管理异步行为，而不是依赖大量线程和阻塞等待。

---

## 17. Guiding Rule

每次准备创建新的 FreeRTOS Task 时，必须先回答：

```text
为什么现有执行上下文无法完成？

是否真的需要并发？

是否真的存在阻塞工作？

Timer / State Machine / Queue 是否已经足够？
```

如果这些问题没有明确答案，则不应该创建新的 Task。
