# Plant V2 完整开发、调试与验证流程

## 1. 文档目的

本文记录 Plant V2 从需求澄清、架构设计、实现、PC 测试、ESP32-C3 交叉编译，到上板、
BLE 绑定和 OTA 验证的完整工程流程，并复盘本轮实际遇到的问题。它同时规定证据边界：

- 主机测试只能证明平台无关策略和协议逻辑。
- ESP-IDF 交叉编译只能证明目标代码可生成，不能证明引脚、电气或无线链路正确。
- 只有板上观测或测量才能证明 USB、GPIO、PWM、BLE、OTA、唤醒和功耗结果。
- 被外部环境阻塞的测试必须写成“未完成”，不能用较低层级测试代替。

V2 产品要求见 [Plant V2 产品需求](../product/product_requirements_v2.md)，硬件验证步骤见
[Plant V2 上板验证清单](../testing/board_bringup_v2.md)。

## 2. 开发前的输入和边界

### 2.1 先把自然语言需求变成可验证语义

本轮需求不是“接几个传感器”，而是四个用户闭环：

1. 电位器提供真实高度，舵机命令不再被当作位置事实。
2. ECM 只判断附近有没有持续讲话及相对音量，不录音、不识别内容。
3. GL5528 只判断相对明亮环境，不能宣称测得准确 lux 或识别真实太阳。
4. AHT21 判断温湿度是否同时适宜。
5. 晒太阳、持续讲话、抚摸、温湿度适宜只产生 `GrowthCredit`，由 Growth Service 统一
   决定是否增长；传感器不能直接控制舵机和灯光。

在写代码前先冻结这些边界，可以避免后续把 ADC、GPIO、PWM 或供应商 API 泄漏进业务层。

### 2.2 明确不能擅自决定的事项

以下值可以先给出保守基线，但必须标记为待样机验证：

- GPIO 和启动绑带电平；
- 舵机脉宽、机构安全端点和堵转电流；
- ECM 增益、包络时间常数和讲话门限；
- GL5528 分压、遮光结构和明亮门限；
- AHT21 安装误差和适宜温湿度区间；
- TP4056 充电电流、负载共享和整机电源预算；
- BLE 天线、发现时延、连接稳定性和深睡眠电流。

编译成功不能解除这些“待验证”标记。

## 3. 仓库与 Git 准备

开始工作时先执行：

```bash
git status --short
git branch --show-current
git log --oneline -5
```

处理原则：

- 当前分支与 V2 感知目标一致时继续使用 `feat/plant-v2-sensing-design`。
- 不暂存 `.vscode` 本地缓存、根目录临时 README、许可证草稿或其他无关文件。
- 每次提交使用显式文件列表，不使用 `git add .`。
- 文档基线、平台无关策略、ESP-IDF 集成和上板修复分别形成可回滚提交。

本轮形成的主要提交顺序是：

```text
4cfa0e7  产品需求、硬件方案、行为映射和 ADR
7dd81d5  Core、Ports、Services 与主机测试
d0a929a  ESP-IDF Adapter、V2 组合根、协议遥测和 BLE 工具
3c77b99  BLE 绑定、硬件说明和上板验收补充
448e25d  上板发现的 GPIO、LEDC 和 GATT 问题修复
```

## 4. 从内向外实现垂直切片

推荐按以下顺序推进，每一步都只依赖更稳定的内层：

```text
产品语义与验收
      ↓
01_core：状态、事件、快照、配置值类型
      ↓
02_ports：位置、声学、光照、温湿度、电量能力契约
      ↓
03_services：检测、迟滞、有界排队、生长/回落与灯效仲裁
      ↓
04_protocol：V2 状态和遥测编码
      ↓
05_adapters：ADC、I²C、LEDC、BLE、NVS、OTA
      ↓
06_bsp：板级连接、电气极性、硬件安全范围
      ↓
07_products：选择 V2 实现、注入配置、推进系统循环
      ↓
08_tests / 09_tools：主机测试、协议测试和板上工具
```

关键检查：

- Core、Ports、Services 不包含 ESP-IDF 和 FreeRTOS 类型。
- Vendor callback 只复制数据、更新连接事实或投递队列，不执行产品行为。
- 业务动画和传感器状态机按 tick 推进，不用延时循环阻塞 System Task。
- OTA、故障、睡眠和 Stop 的优先级高于普通互动生长。
- 位置反馈是机械事实；最后舵机命令和待处理增长不写入 NVS。

## 5. clangd 跳转和 ESP-IDF 头文件

### 5.1 为什么会出现 `freertos/FreeRTOS.h file not found`

clangd 不会因为系统安装了 ESP-IDF 就自动知道组件头文件。它需要当前目标真实生成的
`compile_commands.json`。主机 CMake 数据库没有 ESP-IDF 的 FreeRTOS、NimBLE、GPIO 和
LEDC include 路径，因此即使真实固件能编译，编辑器仍可能报错。

另一个容易忽略的问题是产品选择。根 CMake 默认构建 V1；用默认命令生成的数据库不会
包含 V2 独有的 Adapter 和组合根。工作区现提供独立的 V1/V2 clangd 任务，默认任务是
“clangd: 更新 ESP-IDF V2 编译数据库”，并显式传入 `PLANT_PRODUCT=v2`。

### 5.2 为 V2 生成正确数据库

```bash
source /home/sx/develop/esp/esp-idf/export.sh
idf.py -B build/esp-idf-v2 \
  -D SDKCONFIG=build/esp-idf-v2/sdkconfig \
  -D IDF_TARGET=esp32c3 \
  -D PLANT_PRODUCT=v2 \
  reconfigure
cmake -E copy_if_different \
  build/esp-idf-v2/compile_commands.json \
  compile_commands.json
```

随后在 VS Code 中重启 clangd。`.clangd` 只移除 clangd 不理解的 GCC 专有选项，真实
ESP-IDF 构建仍保留这些选项；`--query-driver` 必须允许 ESP32-C3 的 RISC-V 编译器。

### 5.3 排查顺序

```bash
test -f compile_commands.json
rg -n "feedback_servo_adapter|aht21_adapter|plant_v2" compile_commands.json
rg -n "FreeRTOS-Kernel/include" compile_commands.json
```

判断方式：

- 没有 V2 源文件：生成数据库时选错产品。
- 没有 FreeRTOS include：使用了主机构建数据库，而不是 ESP-IDF 数据库。
- 数据库正确但仍报错：检查 clangd 是否读取工作区根目录数据库并重启索引。
- `idf.py build` 也失败：这是实际构建错误，不能归因于编辑器。

## 6. 主机与协议验证

### 6.1 普通主机测试

```bash
cmake -S . -B /tmp/plant-v2-host -DBUILD_TESTING=ON
cmake --build /tmp/plant-v2-host --clean-first --parallel
ctest --test-dir /tmp/plant-v2-host --output-on-failure
```

覆盖重点包括：

- 讲话开始/结束迟滞和持续讲话窗口；
- 明暗双门限、确认时间和照射积分；
- 温湿度必须同时适宜；
- 每个有效互动的 Growth Credit、有界待处理队列、容量拒绝、限幅和无互动衰减；
- 位置故障、Stop、睡眠、OTA 和行为优先级；
- V1/V2 协议兼容和损坏帧拒绝。

### 6.2 ASan/UBSan

涉及固定缓冲区、协议解析、状态机或所有权变化时增加 Sanitizer 构建。某些受 ptrace
约束的容器无法运行 LeakSanitizer；此时可以只关闭泄漏检测，但必须明确记录：

```bash
env ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/plant-v2-sanitized --output-on-failure
```

这表示 ASan/UBSan 已运行，不表示 LeakSanitizer 已完成。

### 6.3 BLE 工具测试

```bash
python3 09_tools/protocol_tools/test_plant_ble_tool.py
```

当前 12 项协议工具测试覆盖 CRC、Ping、V1/V2 响应、损坏响应拒绝，以及 BlueZ 扫描、
已绑定对象回退和通知订阅重试。

## 7. ESP32-C3 交叉编译

V1 和 V2 都要构建，因为共享 Adapter 的修复可能破坏旧产品：

```bash
source /home/sx/develop/esp/esp-idf/export.sh

idf.py -B /tmp/plant-v1-idf \
  -D SDKCONFIG=/tmp/plant-v1-idf/sdkconfig \
  -D IDF_TARGET=esp32c3 \
  -D PLANT_PRODUCT=v1 \
  build

idf.py -B /tmp/plant-v2-idf \
  -D SDKCONFIG=/tmp/plant-v2-idf/sdkconfig \
  -D IDF_TARGET=esp32c3 \
  -D PLANT_PRODUCT=v2 \
  build
```

同时检查：

- 目标确实为 ESP32-C3；
- `CONFIG_BT_NIMBLE_NVS_PERSIST=y`；
- 最大连接数和绑定数与产品配置一致；
- OTA 双分区存在且镜像有足够余量；
- V1/V2 的共享 LEDC、BLE、Power Adapter 都被重新编译。

本轮最终 V1、V2 镜像分别保留约 72% 和 70% 的最小应用分区空间。

## 8. 烧录和基础上板

### 8.1 烧录与监视

```bash
source /home/sx/develop/esp/esp-idf/export.sh
idf.py -B /tmp/plant-v2-idf -p /dev/ttyACM0 flash
idf.py -B /tmp/plant-v2-idf -p /dev/ttyACM0 monitor
```

如果串口无权限，先检查设备节点、用户组或临时 ACL；不要把权限失败误认为固件没有启动。
监视器打开会通过 USB 控制线复位设备，这是重新看到完整 Boot 日志的正常方式。

### 8.2 没有外设时能证明什么

裸 ESP32-C3 可以验证：

- Bootloader、分区表和应用镜像可启动；
- 原生 USB 持续枚举并输出日志；
- BLE 初始化、广播、连接回调和 NVS 基础行为；
- 未连接传感器时进入明确故障，而不是伪造正常数据。

它不能验证舵机行程、ADC 标定、AHT21/MAX17048、RGB、振动、触摸、电池、睡眠电流或
传感器抗干扰。裸板没有位置反馈，因此仍会因关键机械安全门显示 `status=fault`；这不表示
所有传感器都是致命条件。当前关键性矩阵已明确：位置反馈故障拦截安全启动；ECM、
GL5528、AHT21 或 MAX17048 故障只禁用对应促进来源，并通过遥测报告，不拖垮 BLE、
OTA 和其余交互。

## 9. 本轮问题复盘

### 9.1 I²C 占用 GPIO18/19，原生 USB 消失

现象：V2 运行后 USB Serial/JTAG 不能稳定使用，烧录和日志体验变差。

分析：先排除固件崩溃，再对照 GPIO 分配发现 AHT21/MAX17048 的 I²C 使用了原生 USB
引脚。LEDC、ADC 或 BLE 都不能解释 USB 引脚被重新复用后的掉线。

解决：I²C 改为 GPIO2/9，GPIO18/19 固定保留给原生 USB；增加所有已分配引脚唯一性、
USB 引脚和 Flash 引脚编译期检查。剩余风险是 GPIO2/9 属于启动相关引脚，I²C 上拉、
BOOT 按键和外设复位电平必须在最终原理图上验证。

### 9.2 TTP223 未连接时输入悬空，可能误触发交互

现象：裸板没有 TTP223 时，GPIO21 可能被误判为持续触摸并反复产生交互。

分析：抚摸输入只应在稳定按下至少 300 ms 并稳定释放后产生一次交互；悬空输入会破坏该不变量。

解决：V2 为该输入启用内部弱下拉，并在清单中要求 TTP223 断开状态至少运行 15 秒不得
误触发。推挽高电平仍要在样机上确认能可靠覆盖弱下拉。

### 9.3 ESP32-C3 多个 LEDC 定时器初始化冲突

现象：舵机、RGB 和振动分别单独配置正常，组合初始化时后面的定时器可能失败。

分析：通道只决定 PWM 输出连接到哪个 GPIO，不决定全局时钟。ESP32-C3 的 LEDC 定时器
共享全局时钟源；`LEDC_AUTO_CLK` 可能为 50 Hz、5 kHz 和 200 Hz 选择不一致来源。

解决：所有共享 LEDC 的 Adapter 统一使用 `LEDC_USE_XTAL_CLK`，各定时器仍保留独立频率
和分辨率。V1/V2 均重新交叉编译，并在板上确认不再出现时钟冲突。

### 9.4 GATT 服务注册与安全模式冲突

现象：NimBLE 注册 GATT 服务时出现资源错误，通知特征无法正常工作。

分析：响应特征虽然主要用于服务端 Notify，但 NimBLE 仍要求 Characteristic 定义完整的
访问回调。只设置 `NOTIFY` 并不等于定义了安全订阅边界。

解决：响应特征增加 `access_cb`，CCCD 订阅要求加密，发送前继续检查 connected、secure
和 bonded。修复后服务注册错误消失。

后续真机又出现 CCCD 或命令写入返回 ATT `Unlikely Error (0x0e)`。串口确认链路已经
`encrypted=1`、`bonded=1`、`key_size=16`，但 `authenticated=0`。原因是设备使用
`NoInputNoOutput` 的 Secure Connections Just Works，本身无法提供 MITM 身份认证；同时
`CONFIG_BT_NIMBLE_SM_SC_ONLY=1` 会把所有要求加密的 GATT 属性提升为安全等级 4，强制要求
`authenticated=1`，两项配置在逻辑上互相矛盾。

最终保留 Secure Connections、NVS 绑定以及命令/CCCD 的加密要求，把 SC Only 设为 0。
这仍是 128 位密钥的加密绑定，但不虚假宣称 Just Works 具备 MITM 认证。若量产需要所有权
证明，应增加物理确认或应用层随机挑战，不能重新打开一个硬件能力无法满足的开关。

### 9.5 “手机记住设备，C3 是否不用记住”的误区

BLE 绑定要求双方都保存长期材料。手机/电脑保存设备身份和密钥，C3 的 NimBLE 把 LTK、
IRK 和 CCCD 等写入 NVS。只让一端记住，重启后无法恢复加密链路。

本轮通过只读导出 NVS，确认存在 NimBLE bond 命名空间；普通重新烧录应用和硬复位后，
都使用原有绑定完成加密重连，状态遥测返回 `secure=true`、`bonded=true`。整片擦除、
精确擦除 NVS 或 NVS 损坏恢复才会让 C3 忘记绑定。

### 9.6 拥挤 BLE 环境中的扫描、连接和配对超时

现象：设备串口已显示持续广播，电脑扫描有时需要几十秒才能出现；连接可能报
`le-connection-abort-by-local`，配对可能报 `AuthenticationTimeout`。C3 日志分别出现
“没有收到连接回调”“已连接但安全协商状态 13 超时”等不同阶段。

分析时必须把链路拆开：

```text
广播是否启动
  → 主机是否发现
    → HCI 是否建立连接
      → SMP 是否完成加密/绑定
        → GATT 是否发现服务
          → CCCD 是否订阅
            → 命令/响应是否成功
```

只有 C3 已记录连接回调，才能继续把问题归因于 SMP/GATT；如果 C3 完全没有连接回调，
优先检查电脑 BlueZ、射频距离、天线、扫描缓存和环境拥塞。刷新电脑蓝牙控制器能改善
缓存问题，但不能作为产品解决方案。

早期整包 OTA 曾被电脑端 BlueZ 链路不稳定阻塞；刷新控制器、显式发现设备并等待加密恢复后，
已完成一次 Fault 状态下的整包实机升级。产品侧的长期改进建议见
[ADR 0002](../adr/0002-v2-power-and-ble-experience.md)。

## 10. BLE 绑定验证流程

1. 确认电脑和 C3 都没有过期的单边绑定；只有明确重配时才删除双方记录。
2. 扫描到 `Plant-V2-C3` 后只执行一次 `pair`，不要对已绑定设备重复配对。
3. 串口必须看到连接和 `encryption change ... status=0`。
4. 主机侧确认 `Paired=yes`、`Connected=yes`、`ServicesResolved=yes`。
5. 读取状态：

```bash
python3 09_tools/protocol_tools/plant_ble_tool.py \
  --address 44:B1:76:07:9F:26 \
  --protocol-version 2 \
  --timeout 20 \
  state
```

工具的 `--address` 会先显式发现并保留 Bleak 设备对象，避免连接阶段再次扫描；如果设备已经
连接并停止广播，Linux 下会回退到 BlueZ 的已知设备对象。`BleakDeviceNotFoundError` 表示
设备既未广播也不在相应 `--bluez-adapter` 的对象缓存中；`le-connection-abort-by-local`
表示电脑本地 BlueZ/控制器中止连接，不能归因于 OTA 分片或 C3 擦除了绑定。
绑定重连时，连接完成可能早于链路加密恢复；工具会对受保护响应特征的 CCCD 订阅执行有限
重试，避免把短暂的 `GATT Unlikely Error` 误判为服务注册或协议错误。

6. 复位 C3，不擦除 NVS；再次连接并确认加密成功。
7. 必要时只读导出 NVS 验证命名空间，但不得把密钥内容写入日志或提交仓库。

## 11. OTA 验证流程

完整 OTA 必须覆盖四条路径：

1. 错误产品、硬件版本、长度、版本或损坏镜像被拒绝。
2. 合法镜像分片传输、校验、切换分区并重启。
3. 新镜像自检成功后在确认窗口内标记有效。
4. 新镜像自检失败、未确认或传输中断时保留/回滚到可用镜像。

V2 还必须覆盖故障恢复入口：`Fault` 已停止机械输出后，安全绑定链路可以直接开始 OTA，
不等待故障舵机完成 Sleep；传输取消或失败恢复到 `Fault`。OTA 期间不得继续轮询故障位置
反馈并把 `Updating` 提前打断。

工具示例：

```bash
python3 09_tools/protocol_tools/plant_ble_tool.py \
  --address 44:B1:76:07:9F:26 \
  --protocol-version 2 \
  --timeout 20 \
  ota /tmp/plant-v2-next.bin \
  --version 0x00020004 \
  --hardware-revision 2
```

命令中的版本必须与本次镜像编译进 `ProductConfig::Product::firmware_version` 的值完全
一致；下方 `0x00020001` 是前一轮 Fault 恢复 OTA 的历史实机证据。

根 `sdkconfig.defaults` 仍是开发联调配置，`signed_image` 元数据不能替代密码学验证。生产候选
必须使用 [V2 上板清单的生产 OTA 签名准入](../testing/board_bringup_v2.md#10-生产-ota-签名准入)
叠加签名校验配置，并在受控环境签名；普通构建启动时也会明确记录签名验证未启用。

当前 V2 传感器校准和体验策略仍来自 BSP/ProductConfig 编译期默认值，运行时配置命令、
范围校验、掉电原子写入和 Storage busy 尚未形成完整纵向切片。NVS 现阶段只承担 NimBLE
绑定持久化，不能把“允许持久化校准”描述成已经实现；在配置协议和恢复策略冻结前继续通过
重新构建/OTA 调整样机参数，且绝不保存最后舵机命令。

2026-08-22 裸板证据：设备先以 `0x00020000`、`Fault/MotionFailure`、`secure=true`、
`bonded=true` 运行，从同一加密绑定链路接收了 586080 字节镜像；`BeginOta` 进入
`Updating/Receiving`，`FinishOta` 进入 `Booting/ReadyToReboot`，重启后报告
`firmware=0x00020001`，并仍为 `secure=true`、`bonded=true`。这证明 Fault 恢复入口、整包
传输、分区切换和绑定保留已经实机通过。

裸板的电位器 ADC 是悬空输入，读数可能随机落入有效区间，也可能报告开路、越界或停转；
因此它不能稳定制造启动自检失败，也不能证明位置反馈正常。本次新镜像在确认窗口内读到有效
位置后被确认，随后运动触发 `Stalled` 并回到 Fault，没有发生硬件回滚。取消/写入失败恢复
Fault 已由主机自动化测试覆盖；自检失败回滚和外设完整条件下的升级确认仍须在可控反馈源或
完整 V2 样机上验证。

## 12. `perf/v2-event-driven-runtime` 分支：降低无效唤醒

### 12.1 为什么单独建立这个分支

上一轮 `fix/v2-review-findings` 在提交 `96a628d` 完成了传感安全、执行器自噪屏蔽、
位置反馈防抖和生产 OTA 配置修复。运行时调度会同时修改 FreeRTOS 等待方式、BLE 回调、
GPIO 中断、传感器恢复节奏和低功耗文档，影响面与“纠正功能 Bug”不同，也需要单独做
触摸/BLE 唤醒和整机电流回归。

因此从 `96a628d` 派生 `perf/v2-event-driven-runtime`，目的如下：

- 性能优化可以独立 Review、合并或回退，不把它追加到已经完成的修复提交中；
- 若 GPIO 通知或低功耗上板验证失败，可以只回退调度改动，不丢失前一轮安全修复；
- 分支名明确表达这是 V2 运行时效率优化，不是新增传感器、修改协议或启用深睡。

### 12.2 原来是什么样子

`app_main()` 本身就是 System Task，进入永久循环后通过 `vTaskDelay()` 阻塞。Task 存在只
保留栈和 TCB，并不等于一直占用 CPU；真正的问题是固定周期唤醒：

| 场景 | 原实现 | 影响 |
| --- | --- | --- |
| Idle/Interacting/Updating/Fault | 固定 20 ms Tick | 每秒执行 50 次完整应用链；Fault 也保持麦克风采样 |
| 稳定浅睡 | 固定 250 ms Tick | 即使没有命令、触摸或传感截止时间也每秒唤醒 4 次 |
| BLE 命令/安全状态 | 回调只入队/改原子状态 | System Task 只能等下一次 Tick 观察到变化 |
| TTP223 | 每个 Tick 读取 GPIO | 把睡眠周期直接变长会漏掉短按或增加触摸延迟 |
| AHT21 未焊/断线 | 上电状态约 100 ms 重试 | 可选传感器持续故障时反而频繁唤醒 CPU 和 I²C |

实际构建为 100 Hz FreeRTOS Tick，20 ms 只相当于 2 Tick，低于当前 Tickless Idle 的
3 Tick 进入门限；活动快循环本身就不能利用长空闲窗口。稳定浅睡虽然可以进入 Tickless
Idle，但固定 250 ms 仍会制造与业务事件无关的周期唤醒。

### 12.3 修改后是什么样子

本分支不创建新的传感器 Task，也不删除/重建 System Task，而是把唯一业务 Task 改成
“通知或最近截止时间到达时运行”：

```text
NimBLE 回调：入队/更新安全状态 ── Task Notification ──┐
TTP223 GPIO：捕获电平并禁用本次中断 ─ FromISR Notify ─┤
触摸防抖截止时间 ──────────────────────────────────────┤
AHT21 非阻塞测量截止时间 ──────────────────────────────┤
1 秒稳定睡眠兜底 Tick ─────────────────────────────────┘
                              ↓
                         System Task
                              ↓
             统一执行协议、生命周期、Service 和输出逻辑
```

关键变化：

- BLE Vendor Callback 仍只复制/入队、更新原子状态和通知，绝不直接执行业务；连续多帧使用
  FreeRTOS 计数通知逐次唤醒，避免通知被清空后队列残留到下一次超时。
- TTP223 使用电平中断捕获按下/释放。ISR 只禁用本次 GPIO 中断并发送通知；System Task
  读取实际电平后把中断改为等待相反电平，Adapter 仅完成防抖，并在稳定触摸至少 300 ms
  后的稳定释放时产生一次交互。
- 稳定浅睡兜底从 250 ms 调到 1 秒；触摸边沿会立即通知，80 ms 防抖截止时间会再次调度，
  因而不能用“1 秒 Tick”推导触摸延迟。
- AHT21 在触发测量后把约 85 ms 完成时间加入最近截止时间，不能因为 1 秒兜底而阻塞等待
  或把读取无条件推迟 1 秒。
- Fault 的警示振动仍用 20 ms 快 Tick 推进；振动结束后改为 500 ms Tick，刚好保持
  `ErrorBlink` 每 500 ms 换相。Fault/Updating/Sleeping 都关闭 ECM 模拟前端采样。
- AHT21 连续三次失败后上报 `SensorFault` 并把恢复探测退到 30 秒；恢复成功后重新进入
  正常 2 秒采样。30 秒是上板前的保守 BSP 值，不是已经验证的最佳参数。
- 固件版本由 `0x00020003` 提升到 `0x00020004`，用于合法 V2 OTA 版本匹配。

### 12.4 明确没有改变什么

- 没有增加一个模块一个 Task，也没有让 ISR/蓝牙回调控制舵机、灯光或生命周期。
- 没有启用自动深睡；手机可发现性策略和 BLE 绑定/NVS 规则保持不变。
- 没有在舵机、RGB 或振动工作时盲目释放 `ESP_PM_NO_LIGHT_SLEEP`。当前 LEDC 输出进入
  Light Sleep 的行为仍需专项验证，不能为了省电破坏 PWM 安全和可见反馈。
- 没有把主机测试或交叉编译写成电流改善证据。1 秒兜底、30 秒故障退避和 GPIO 通知都
  必须按 V2 上板清单测量响应时间、平均电流和恢复可靠性。

### 12.5 本分支验证记录

- 主机 Debug 构建与全部领域/集成测试通过；新增覆盖运行状态周期选择、Fault/OTA 关闭
  麦克风以及三次快速失败后进入 30 秒退避。
- ESP32-C3 V2 新鲜交叉编译通过，`plant_v2.bin` 为 `0x90360` 字节；最小 OTA 应用分区
  `0x1e0000`，剩余 `0x14fca0`（70%）。
- ESP32-C3 V1 兼容构建通过，证明给 ESP Adapter 配置增加的可选通知回调没有破坏 V1；
  生产 OTA 签名配置构建也通过，输出等待密钥签名的 `0xa0000` 字节镜像，分区余量 67%。
- GPIO 按下/释放通知、BLE 轻睡连接唤醒、Fault 电流和 AHT21 断线恢复仍属于上板项目；
  在这些项目完成前只能声称软件路径和目标构建通过。

## 13. 每次交付前的检查表

```bash
git diff --check
python3 09_tools/protocol_tools/test_plant_ble_tool.py
```

并完成：

- 主机测试；
- 必要的 ASan/UBSan；
- V1 与 V2 新鲜 ESP-IDF 构建；
- 镜像和分区余量检查；
- 文档相对链接检查；
- `git diff` 与 `git diff --cached` 人工复查；
- 显式列出已验证、未验证和被阻塞项目；
- 只暂存本次文件，再创建 Conventional Commit。

## 14. 当前遗留项

- BLE 已实现启动/断连后 30 秒快速广播再转慢速广播，但拥挤环境 P95 发现时延、应用重试
  和物理配对窗口仍需完整样机与手机应用共同验收。
- 自动深睡已默认关闭；是否引入按供电/电量自适应深睡，仍需整机电流与用户重连时延数据。
- 非关键传感器已支持功能降级，但量产应用仍需把 `SensorFault` 显示成可理解的维护提示。
- Fault 状态下的 OTA 整包传输、分区切换和绑定保留已实机通过；自检失败回滚、传输中掉电
  以及完整外设条件下的成功确认仍需专项样机验证。
- GPIO2/9、GPIO8/10、GPIO20/21 的启动、下载和外设隔离仍等待最终原理图确认。
