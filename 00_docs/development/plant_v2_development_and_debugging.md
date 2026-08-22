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
03_services：检测、迟滞、冷却、生长与灯效仲裁
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
- Growth Credit 冷却、限幅、过期和单个待处理量；
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

本轮 6 项协议工具测试通过，覆盖 CRC、Ping、V1/V2 响应和损坏响应拒绝。

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

### 9.2 TTP223 未连接时输入悬空，可能误清绑定

现象：裸板没有 TTP223 时，GPIO21 可能被误判为持续触摸；10 秒后会执行本地 BLE 恢复。

分析：绑定看似“无缘无故丢失”时，不能只检查 NVS；还要追踪所有显式删除绑定的入口。
本项目既支持 `ForgetBonds` 命令，也支持持续触摸 10 秒恢复，悬空输入会触发后者。

解决：V2 为该输入启用内部弱下拉，并在清单中要求 TTP223 断开状态至少运行 15 秒不得
误恢复。推挽高电平仍要在样机上确认能可靠覆盖弱下拉。

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
精确擦除 NVS、NVS 损坏恢复、`ForgetBonds` 或本地 10 秒恢复才会让 C3 忘记绑定。

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

本轮最终的整包 OTA 传输被电脑端 BlueZ 链路不稳定阻塞，因此没有宣称 OTA 实机通过。
产品侧的改进建议见 [ADR 0002](../adr/0002-v2-power-and-ble-experience.md)。

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

工具示例：

```bash
python3 09_tools/protocol_tools/plant_ble_tool.py \
  --address 44:B1:76:07:9F:26 \
  --protocol-version 2 \
  --timeout 20 \
  ota /tmp/plant-v2-next.bin \
  --version 0x00020001 \
  --hardware-revision 2
```

注意：当前无外设裸板会因为 V2 启动自检失败而拒绝确认新镜像或触发回滚，适合验证回滚
路径，不适合证明一次完整的“升级成功”。合法升级成功必须使用外设齐全、传感器自检可通过
的 V2 样机；也可以先用 V1 基线单独验证 BLE 传输链路，但不能把它当作 V2 全量验收。

## 12. 每次交付前的检查表

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

## 13. 当前遗留项

- BLE 已实现启动/断连后 30 秒快速广播再转慢速广播，但拥挤环境 P95 发现时延、应用重试
  和物理配对窗口仍需完整样机与手机应用共同验收。
- 自动深睡已默认关闭；是否引入按供电/电量自适应深睡，仍需整机电流与用户重连时延数据。
- 非关键传感器已支持功能降级，但量产应用仍需把 `SensorFault` 显示成可理解的维护提示。
- OTA 整包传输、确认和掉电回滚仍需在稳定 BLE 主机和完整 V2 外设样机上闭环。
- GPIO2/9、GPIO8/10、GPIO20/21 的启动、下载和外设隔离仍等待最终原理图确认。
