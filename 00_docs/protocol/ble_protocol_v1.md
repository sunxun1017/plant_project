# Plant V1 BLE 二进制协议

## 1. GATT 定义

| 项目 | 值 |
| --- | --- |
| 设备名 | `Plant-V1-C3` |
| Service UUID | `0xFFF0` |
| Command Characteristic | `0xFFF1`，Write / Write Without Response |
| Response Characteristic | `0xFFF2`，Notify |
| 首选 MTU | 517 |
| 最大协议帧 | 512 字节 |

手机连接后应订阅 `0xFFF2`，再向 `0xFFF1` 写入完整协议帧。V1 不支持把一个协议帧拆成多次 GATT Write。

## 2. 字节序与帧结构

所有多字节整数均为 little-endian。

| 偏移 | 长度 | 字段 | 说明 |
| --- | --- | --- | --- |
| 0 | 1 | Magic | 固定 `0xA5` |
| 1 | 1 | Version | V1 固定 `0x01` |
| 2 | 1 | Type | 命令类型；响应固定 `0x80` |
| 3 | 1 | Flags | V1 固定为 0 |
| 4 | 2 | Request ID | 由客户端生成，响应原样返回 |
| 6 | 2 | Payload Length | 0–500 |
| 8 | N | Payload | 命令或响应内容 |
| 8+N | 4 | CRC32 | 对 Header 和 Payload 计算 IEEE CRC32 |

CRC32 参数：初始值 `0xFFFFFFFF`，反射多项式 `0xEDB88320`，结果按位取反。

## 3. 命令

| Type | 名称 | Payload |
| --- | --- | --- |
| `0x01` | Ping | 无 |
| `0x02` | GetState | 无 |
| `0x03` | SetBehavior | 1 字节 Behavior |
| `0x04` | StopBehavior | 无 |
| `0x10` | BeginOta | 17 字节 OTA Metadata |
| `0x11` | OtaChunk | 4 字节 offset + 1–496 字节固件数据 |
| `0x12` | FinishOta | 无 |
| `0x13` | CancelOta | 无 |

Behavior：

| 值 | 行为 |
| --- | --- |
| 0 | WakeUp |
| 1 | Happy |
| 2 | Attention |
| 3 | Calm |
| 4 | Sleep |
| 5 | Error |

## 4. BeginOta Payload

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 4 | Product ID，Plant V1 为 `0x504C414E` |
| 4 | 4 | Hardware Revision，当前为 1 |
| 8 | 4 | Firmware Version，单调递增整数 |
| 12 | 4 | Image Size |
| 16 | 1 | Signed Image，正式包必须为 1 |

OTA 分片必须从 offset 0 开始严格连续发送。乱序、重叠、空分片和超过声明镜像大小的分片会被拒绝。V1 中断后从头重新传输，不支持断点续传。

## 5. 响应 Payload

所有有效命令都通过 `0xFFF2` 返回 Type `0x80` 的通知。

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 1 | 原命令 Type |
| 1 | 1 | ErrorCode，0 表示成功 |
| 2 | 1 | DeviceState |
| 3 | 1 | PowerMode |
| 4 | 1 | 当前 Behavior |
| 5 | 1 | OtaState |
| 6 | 4 | 已接收 OTA 字节数 |
| 10 | 4 | 当前 Firmware Version |

客户端必须使用 Request ID 匹配请求和响应，不能假设通知顺序等于业务完成顺序。行为命令成功表示设备接受并开始执行，不表示机械运动已经完成。

## 6. 错误处理

- Magic、Version、Flags、长度或 CRC 错误的帧直接丢弃，不执行硬件动作。
- 格式正确但参数非法的命令返回对应 ErrorCode。
- 接收队列满时 GATT Write 返回资源不足，客户端应退避后重试。
- OTA 命令在非 `Updating` 状态、普通行为在 `Updating` 状态均返回状态错误。
- `FinishOta` 成功响应发出后，设备等待约 100 ms 再重启。

## 7. 当前安全边界

当前 GATT 服务用于开发联调，尚未强制配对、链路加密和应用层鉴权。生产版本启用远程 OTA 前必须完成：

- BLE Secure Connections 与绑定策略。
- 正式固件签名验证配置和密钥烧录流程。
- 防重放或会话授权策略。

客户端提供的 `Signed Image` 标志不是密码学证明；最终可信性必须由 Bootloader/ESP-IDF 镜像签名验证保证。
