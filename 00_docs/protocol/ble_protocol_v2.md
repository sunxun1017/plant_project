# Plant V2 BLE 二进制协议

## 1. 兼容与安全

V2 复用 V1 的 UUID、帧头、小端字节序和 CRC32。客户端把帧头 `Version` 设置为 `0x02`；
固件仍接受 V1 请求并返回 14 字节 V1 响应，因此旧工具不会误读新增字段。

连接后设备发起 LE Secure Connections 配对，绑定密钥由 NimBLE 持久化到独立 `nvs`
分区。正常复位、意外掉电与 `ota_0`/`ota_1` 切换不擦除该分区。命令和响应都要求链路
已加密且当前对端已绑定。无显示/键盘版本使用 Just Works，所以不具备 MITM 身份认证。

最多保存 3 个绑定且同时只连接 1 台主机。V2 不提供触摸或 BLE 解绑入口，也不会自动淘汰
旧绑定。擦除整片 Flash 或 NVS 会丢失绑定，这属于受控维护/重新烧录行为。

## 2. 命令

V1 的全部命令在 V2 继续有效；`SetBehavior` 仍只接受 0–5，`Grow` 和 `Retract` 只能
由 Growth Service 的有效互动与无互动衰减策略触发，客户端不能绕过位置闭环和安全限幅。

| Type | 名称 | Payload | 说明 |
| --- | --- | --- | --- |
| `0x01` | Ping | 无 | 连通性检查 |
| `0x02` | GetState | 无 | 获取完整 V2 遥测 |
| `0x03` | SetBehavior | 1 字节 | 行为 0–5 |
| `0x04` | StopBehavior | 无 | 停止当前行为/生长运动 |
| `0x10`–`0x13` | OTA | 同 V1 | 开始、分片、完成、取消 |

## 3. V2 响应

响应帧 Type 为 `0x80`，Payload 固定 54 字节。偏移均相对于 Payload 起点；0–13 与 V1
完全相同。

| 偏移 | 长度 | 字段 | 单位/编码 |
| ---: | ---: | --- | --- |
| 0 | 14 | V1 基础响应 | 请求、错误、生命周期、行为、OTA、固件版本 |
| 14 | 4 | Capabilities | 位图，见下表 |
| 18 | 2 | Actual Position | 0–1000 实测生长高度 |
| 20 | 2 | Target Position | 0–1000 当前目标高度 |
| 22 | 1 | Position Feedback | 0 unavailable，1 valid，2 open，3 short，4 out-of-range |
| 23 | 1 | Motion Fault | 0 none，1 feedback，2 stalled，3 opposite，4 timeout |
| 24 | 1 | Motion Flags | bit0 moving，bit1 target reached |
| 25 | 1 | Acoustic State | 0 quiet，1 speaking，2 sustained，3 fault |
| 26 | 2 | Volume | 0–1000 相对音量 |
| 28 | 2 | Noise Floor | 0–1000 自适应底噪 |
| 30 | 2 | Speaking Duration | 秒，饱和到 65535 |
| 32 | 1 | Illumination State | 0 dark，1 ambient，2 bright，3 fault |
| 33 | 2 | Relative Light | 0–1000，不代表 lux |
| 35 | 2 | Bright Duration | 秒，饱和到 65535 |
| 37 | 1 | Climate State | 0 cold，1 hot，2 dry，3 humid，4 suitable，5 fault |
| 38 | 2 | Temperature | 有符号 0.01 °C |
| 40 | 2 | Humidity | 0.1 %RH |
| 42 | 2 | Suitable Duration | 秒，饱和到 65535 |
| 44 | 1 | Battery State | 0 unavailable，1 normal，2 low，3 critical，4 fault |
| 45 | 2 | Battery Level | 千分比，1000 = 100% |
| 47 | 2 | Battery Voltage | mV |
| 49 | 1 | Recent Growth Source | 0 none，1 touch，2 speech，3 light，4 climate，5 inactivity decay |
| 50 | 1 | Pending Growth Source | 同上 |
| 51 | 1 | Growth Flags | bit0 pending，bit1 at limit，bit2–4 待处理队列数量 0–4 |
| 52 | 1 | BLE Flags | bit0 encrypted，bit1 bonded |
| 53 | 1 | Active Fault | `ErrorCode`；0 表示无活动故障 |

Capabilities 位图：bit0 位置反馈、bit1 声学活动、bit2 相对光照、bit3 温湿度、bit4 生长
策略、bit5 电池、bit6 持久绑定。客户端必须先检查能力位，再解释对应字段和状态；传感器
故障时使用枚举状态，不能把零值解释为正常测量。

基础响应的 Behavior 值新增 7 `Retract`，表示无互动衰减运动；6 `Grow` 仍表示互动促进
运动。它们只会作为状态上报，`SetBehavior` 不接受 6 或 7。

## 4. 工具

`python3 09_tools/protocol_tools/plant_ble_tool.py state` 默认发送 V2 请求并打印温湿度、
光照、交流状态、故障、电量、实测高度和绑定状态。需要兼容性验证时可传
`--protocol-version 1`。
