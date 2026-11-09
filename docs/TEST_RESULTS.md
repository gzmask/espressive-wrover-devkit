# 测试记录

硬件初检：2026-11-07 01:14 UTC；当前验收截止2026-11-08。状态采用PASS / FAIL / NOT TESTED；历史未实现/旧端口只属于对应版本，不能当作当前限制或新写入许可。

## 当前验收摘要：0.1.7-ota-wifi / B / VALID

- **PASS** — 唯一真实HTTPS POST/200，软件重启到0.1.7并30秒健康确认；另一次正常复位180.024秒及证书校验的GET/status内容hash/槽/online/healthy/confirmed通过。
- **PASS** — PSRAM有限自检、固定ILI9341、US手动策略/首次DHCP、心跳及刷新日志；原始翻转根因未确认，不冒充全程目视检查。
- **PASS** — 完整4MiB重建预期、B及全部补集、控制CRC/VALID和8安全字段；核验发生在最终正常启动前。所有写入许可已消耗。
- **软件PASS** — 45 C ASan/UBSan、15 Python、4 uploader mock、四配置构建；mock不是实机证明。
- **代码边界** — GET/status、GET/health不要求Bearer；POST/ota要求Bearer，客户端校验TLS服务端证书。已实测GET/status，GET/health只确认同handler路由，未独立实机调用。
- **NOT TESTED** — 故障回退、受控断电、真实掉线恢复、新十分钟长测、WPA3细节、完整8MiB、JTAG/备份恢复。完整流程见 [OTA.md](OTA.md)。

## 历史里程碑0：初始硬件/原始备份

- [x] **PASS** — USB 双接口枚举：FTDI 0403:6010；ioreg 对应 interface 0/1 与两个串口。
- [x] **PASS** — UART 映射：`/dev/cu.usbserial-21401`，115200 8N1。
- [x] **PASS** — 原有启动日志采集：被动 8 秒 326 字节；复位后 12 秒 329 字节。日志见 [HARDWARE.md](HARDWARE.md)。这不是应用健康验证。
- [x] **PASS** — ROM 自动下载模式通信：`chip-id` 成功，ESP32-D0WD v1.0。
- [x] **PASS** — `flash-id` 成功：4 MB，manufacturer 0x20，device 0x4016。
- [ ] **NOT TESTED（受控验收）** — 用户报告已操作 BOOT + EN，但当时 USB 未枚举，未同步验证 ROM 通信；后续自动 RTS/ROM 通信成功不能冒充手动流程验收。
- [ ] **NOT TESTED** — JTAG 实际调试与跳线状态。
- [x] **PASS** — PCB 版本照片核对：LCD 面右上角丝印 ESP-WROVER-KIT_V4.1，证据 `.local/evidence/board-lcd-side-v4.1.png`。
- [x] **PASS** — v4.1 LCD 引脚资料核对：官方指南、原理图与官方示例相符；后续已按固定接线运行并取得工作照片，未试探其他 GPIO。
- [ ] **NOT TESTED** — 模组标记清晰核对；现有照片反光，仍以交接资料型号为待验证项。
- [x] **PASS** — LCD indexed ID4 两轮均为 93 41，匹配 ILI9341；320×240 配置下的实际工作照片显示完整边框、未镜像横屏文字及正确基本色序，用户另行确认白色正常。
- [x] **PASS** — PSRAM 运行时识别为 8 MiB，映射 4 MiB，40 MHz；内置启动测试和 128 KiB 六模式自检通过，见 PSRAM_PROBE.md。
- [x] **PASS** — 115200 下两次完整读取 4 MB，cmp 和 SHA-256 一致，文件权限 0600 / 目录 0700，Git 忽略。
- [ ] **FAIL（高速读取）** — 460800 baud 读出报数据包损坏，未生成可用备份；改用 115200 后两次通过，高速故障根因未定位。
- [x] **PASS** — 安全字段只读检查：Secure Boot / Flash 加密未启用，没有 eFuse 修改。
- [x] **PASS** — 原 bootloader / factory 镜像 checksum/hash 及 8 项分区表 MD5 校验；factory 是唯一有效应用槽。
- [ ] **FAIL（旧布局语义）** — 现代分区校验器拒绝旧表中的多个 data/ota 子类型；不代表 MD5 损坏。该初检阶段未迁移；后续0.1.6已另获批准完成双槽迁移，不能将旧表FAIL算作当前布局错误。
- [x] **PASS** — 备份后正常复位，15 秒取得 329 字节原启动日志与 > 提示符；没有发送 REPL 命令。
- [ ] **NOT TESTED** — 备份恢复写入；没有为了验证而破坏原固件。
- [x] **PASS** — ESP-IDF v5.5.6 / `880a203ae603d08a94d06aa01e0d6761958e5e9e` 与工具链安装、激活、编译。版本见 [BUILD.md](BUILD.md)。

## 历史里程碑1：0.1.0 ESP-IDF / FreeRTOS

- [x] **PASS** — 默认 esp32 固件从 fullclean 完整编译，编译日志无 warning/error。4 MB、DIO/40 MHz、最低 revision 1.0。
- [x] **PASS** — 应用镜像头与校验检查：ESP32、firmware 0.1.0、IDF v5.5.6，checksum/hash valid。
- [x] **PASS** — PSRAM 可选分支独立编译；仅覆盖条件代码，不是硬件探测。
- [x] **PASS** — clang-format 18.1.8 检查通过。
- [x] **PASS** — 状态组件 Clang 静态分析，以及两个自有 C 源文件的 Xtensa GCC -fanalyzer，无诊断。
- [x] **PASS** — 配置保护：错误 esp32s3 目标、不匹配的 IDF commit 均被 CMake 拒绝。
- [x] **PASS** — 8 项主机测试，启用 ASan/UBSan：初态、正常/停滞/恢复、乱序/重复、JSON 字符串转义、缓冲区容量 0..512、无效输入、整数边界/计数饱和、逐分配点 OOM 故障注入与释放检查。
- [x] **PASS** — 经用户明确授权，仅写 factory 应用，173328 字节，实际擦写 `[0x10000, 0x3B000)`；写入 1 次且数据哈希校验通过。
- [x] **PASS** — 写前 NVS 变化被检测并处理：最新前缀双读一致，重建当前快照并通过整片设备摘要校验后才写入。
- [x] **PASS** — 写后及运行后，新镜像、全部授权擦写范围外 Flash 数据与已检查安全字段均校验一致。
- [x] **PASS** — 原 bootloader 加载新应用，三次正常 RTS 复位启动均成功，目标/版本正确。
- [x] **PASS** — 实机双任务/队列，心跳接收计数 1→327；CPU 160 MHz，Flash 4194304 字节，稳定空闲 heap 300224 字节。
- [x] **PASS** — 连续采集 330.15 秒，running 样本跨度 325.993 秒，未见 panic/watchdog/stalled/意外复位。
- [x] **PASS** — 0.1.0 基线当时未启用 PSRAM；后续 0.1.1-psram-probe 已实测识别容量和有限读写，见下节。
- [ ] **NOT TESTED（受控断电验收）** — USB 插拔期间的电源状态未受控记录。07:44 UTC USB 恢复后，已只读确认同一设备、UART `/dev/cu.usbserial-21301`、旧版 PSRAM 自检通过及 18.07 秒心跳增长；不能外推为受控断电恢复验收，见 HARDWARE.md。

## PSRAM 探测版：0.1.1-psram-probe 已独立验证

以上首次基础固件记录针对 **0.1.0**；以下是另行授权部署后的 **0.1.1-psram-probe** 实测结果，不是沿用旧版结论。

- [x] **PASS** — 新源码的无 PSRAM 0.1.1 分支与 PSRAM 诊断分支都编译成功，无编译警告。
- [x] **PASS** — 原 8 项测试与新增 4 项内存模式测试，共 12/12，ASan/UBSan 开启。
- [x] **PASS** — 模式已知值、128 KiB 缓冲区及保护字、逐位置损坏/首错索引、无效范围均通过主机测试。
- [x] **PASS** — clang-format、内存算法 Clang 分析、四个自有 C 源文件 GCC -fanalyzer 无诊断；实际编译响应文件包含 revision 1 cache workaround / MEMW 参数。
- [x] **PASS** — 镜像头及 checksum/hash 校验，220528 字节，SHA-256 `3e8130cd4b665aa41c3a75a733ad551cf601f08ab2a14127ffc208edff9bb3ef`。
- [x] **PASS** — 明确获准后仅写应用 `[0x10000, 0x46000)`，一次成功，工具报告 10.3 秒且数据哈希校验通过。
- [x] **PASS** — 写前 0.1.0 整片快照与设备摘要一致；写后和运行后，新镜像、全部范围外 Flash 与已检查安全字段均通过校验。
- [x] **PASS** — 实测 PSRAM 器件 8388608 字节、allocator 4194304 字节；外部空闲量自检前后均为 4192136 字节。
- [x] **PASS** — 三次正常复位均经原 bootloader 成功启动，内置启动测试及 128 KiB 六模式自检均通过。
- [x] **PASS** — 连续采集 330.12 秒，running 样本跨度 325 秒，心跳 1→326，无 panic/watchdog/stalled/意外复位。稳定总空闲 heap 4492480 字节，其中内部 300344 字节。
- [ ] **NOT TESTED** — 未映射的高 4 MiB、整个 PSRAM 的全字节测试和长时间连续读写压力测试；本次不可外推为全部 8 MiB 完全验证。

每次启动有一条预期的 4 MiB 映射限制警告，其他警告/错误未见。配置、精确命令、真实串口摘录和完整证据路径见 [PSRAM_PROBE.md](PSRAM_PROBE.md)。

## 里程碑 2：LCD（0.1.2-lcd-probe 已部署并实测通过）

- [x] **PASS（资料）** — 官方 BSP 明确支持 ILI9341 / ST7789 两种出货控制器；v4.1 引脚与官方 SPI 示例核对一致。用户要求受控尝试候选，不代表已确认实际面板型号。
- [x] **PASS（软件）** — LCD+PSRAM、LCD 无 PSRAM、默认两者均关闭，三个配置均编译成功，无 warning/error；官方 ILI9341 2.1.0 及间接依赖已锁定。
- [x] **PASS（主机）** — 新增 LCD 8 项测试，总计 20/20，ASan/UBSan 开启；覆盖签名识别与否定结果、候选计划、RGB565 字节序、容量/坐标边界、文字、动态标记与条带一致性。
- [x] **PASS（静态）** — clang-format、显示逻辑 Clang 分析、六个自有 C 文件 GCC -fanalyzer 无诊断；未确认 PCB 的编译保护按预期报错。
- [x] **PASS（镜像）** — 273792 字节，目标 ESP32、版本 0.1.2-lcd-probe，checksum/hash 有效；SHA-256 `8b9c24dd2fbbce9eb899588c88864363c5dd4f732aa87d4b3219b104c3bab67f`。
- [x] **PASS** — 新范围另行获准后，仅写应用 `[0x10000, 0x53000)`，一次成功，工具报告 12.9 秒、hash 校验通过；没有沿用旧范围授权。
- [x] **PASS（写后及运行后）** — 新镜像、全部范围外 Flash 和已检查的 8 项安全字段均校验一致。
- [x] **PASS** — 三次正常 RTS 复位均经原 bootloader 启动新应用，PSRAM 启动/128 KiB 六模式自检每次通过，每次两轮 indexed ID4=93 41，选择 ILI9341，未试 ST7789。
- [x] **PASS（照片 + 操作者）** — 背光、320×240 横屏配置完整边框、未镜像文字、RGBW 顺序；用户确认白色正常。照片原件和观察记录见 LCD_PROBE.md。
- [x] **PASS（串口并发）** — 连续采集 660.16 秒，同一 ILI9341 的提交日志跨度 649.99 秒、计数 1→651；心跳样本跨度 655 秒、计数 1→656，无异常复位、panic/watchdog/stalled 或队列丢样。
- [x] **PASS（内存）** — 稳定总空闲 heap 4473280 字节，内部 281144 字节，最低总 heap 4337840 字节；LCD 栈低水位 4448 字节、状态任务 1824 字节。
- [x] **PASS（最终状态）** — 运行后核验完成后正常复位，第三次采集 35.16 秒通过，设备留在应用运行态，没有重复烧录。

详细方案、构建命令、软件证据和候选冻结路径见 [LCD_PROBE.md](LCD_PROBE.md)。本轮软件通过不能替代上述实机结果，也不宣称完成 Wi-Fi/LCD 并发验收。

## 里程碑 3：Wi-Fi / HTTP

### 0.1.3-wifi-scan（扫描与 LCD/心跳并发实测通过）

- [x] **PASS（设计/配置）** — 同时关闭 Wi-Fi NVS、PHY 校准持久化及 PHY 数据分区，初始化标志 nvs_enable=0、RAM storage；不初始化或擦除旧 NVS，不改分区表。
- [x] **PASS（构建）** — LCD+PSRAM+扫描、LCD+扫描无 PSRAM、默认全部关闭，三个配置编译成功，无 warning/error；默认 ELF 不含 Wi-Fi init/start 或 TCP/IP init 符号。
- [x] **PASS（主机）** — 新增扫描逻辑 7 项及 LCD 网络渲染 1 项，合计 28/28，ASan/UBSan；先失败后通过的日志已保存。
- [x] **PASS（静态/保护）** — clang-format、两个纯逻辑 Clang 分析、8 个自有 C 源文件 GCC -fanalyzer 无诊断；三个持久化选项分别开启时，编译保护均按预期拒绝。
- [x] **PASS（镜像）** — ESP32 / 0.1.3-wifi-scan，855312 字节，checksum/hash 有效；SHA-256 `e458183c39616b2cba23cb8f01110c18e71ef5189d26b7cbd1cbc15808988b6a`。
- [x] **PASS（授权/写入）** — 用户明确确认本冻结镜像和 `[0x10000, 0xE1000)` 后回复“yes”；只写应用一次，用时 46.9 秒、数据 hash 校验通过，没有沿用旧 LCD 授权。
- [x] **PASS（保护）** — 写前当前 4 MB 重建快照与设备全片摘要匹配；写后、串口中断后及完整 690 秒运行后，镜像、全部范围外 Flash（包括旧 NVS）及 8 项安全字段均一致。
- [x] **PASS（初次启动/显示）** — 原 bootloader 加载 0.1.3；PSRAM 启动测试和 128 KiB 六模式自检通过，两轮 ID4=93 41；首轮扫描 8 个 AP。用户确认看见 `AP:8, SCANS:1`，不要求照片。
- [ ] **INCOMPLETE（原长时采集）** — 约 557 秒、9 轮扫描后 UART 报 `Device not configured`，没有完成元数据，不能算 690 秒 PASS；中断前心跳/LCD/轮间 heap 正常，未见固件异常。端口重枚举为 20301 后确认同一设备；中断记录独立保留，未重复烧录。
- [x] **PASS（完整长时重测）** — 新日志采集 690.135 秒 / 11 轮扫描，138 个 running 样本，心跳 1→687、跨度 685.994 秒；69 个 ILI9341 提交记录，计数 1→681、跨度 679.990 秒，无异常复位/panic/watchdog/stalled/队列丢样。
- [x] **PASS（扫描/内存）** — 各轮 4–7 个 AP，共 58 条观测（不是 58 个独立 AP）；结果信道 1/3/6/9、RSSI -92..-46 dBm。126 个轮间 idle 样本的总 heap 均为 4391744、内部 199836 字节；最低总 heap 4258552 字节。LCD/Wi-Fi/状态栈低水位分别 4088/3624/1576 字节。
- [x] **PASS（完整启动与收尾）** — 三次完整采集启动分别 45.17 / 690.14 / 45.17 秒，均通过 PSRAM、自检、两轮 ILI9341 ID 和扫描检查。运行后保护复核完成，再正常 RTS 启动最终 45 秒采集通过，设备保持运行、UART 无占用；另一次中断采集不混入完整时长验收。

方案、重现命令、局限及证据见 [WIFI_SCAN.md](WIFI_SCAN.md)。以下均为硬件/后续功能验收，不能由软件 PASS 代替。

- [x] **PASS（扫描结果）** — 2.4 GHz 被动扫描已有多轮 AP 结果；数据格式、信道 1–11、RSSI 和安全类型检查通过。用户后续提供本地配置后，已私下将目标 SSID 匹配到历史 WPA2-PSK 记录，不回显名称；这不等于凭据正确性/加密协商验收。
- [x] **PASS（0.1.4 实机）** — 指定网络关联、DHCP、在线并发与 RAM-only 凭据复位行为，见下节；真实掉线退避重连仍 **NOT TESTED**。
- [ ] **历史0.1.3未实现** — 该扫描镜像没有HTTP；当前HTTPS GET/status已实测，GET/health路由存在但未独立调用验收。
- [ ] **NOT TESTED** — 断电/重连恢复及 LCD 状态同步。
- [x] **PASS** — 可复用的通用状态转换与 JSON 格式单元测试；见里程碑 1 的 8 项测试。
- [x] **PASS（主机）** — 扫描专用状态转换、边界/计数饱和、SSID 安全编码、BSSID 遮蔽、JSON/OOM 和 LCD 网络状态渲染。
- [x] **PASS（主机）** — STA 连接状态转换、有界退避及 DHCP/丢地址模型；实际关联/IP 事件已验证；真实掉线与恢复仍未验证。该0.1.4当时没有HTTP schema/响应；当前HTTPS状态由独立ota_service提供。

### 0.1.4-wifi-sta（连接/DHCP、在线并发及保护实测通过）

- [x] **PASS（输入边界）** — 用户明确确认美国所在地；本地文件权限 0600、两行格式有效，目标 SSID 私下匹配历史 WPA2-PSK AP，不回显凭据。
- [x] **PASS（主机）** — 41/41 C 测试，ASan/UBSan；11/11 Python 工具测试，全部使用合成凭据和模拟串口。
- [x] **PASS（模型/协议）** — 关联与 DHCP 分离、丢 IP 清理、超时、有界退避与稳定窗口、防短连风暴、计数/时钟边界、续租非法地址不修改状态、JSON/OOM；凭据帧长度/CRC/令牌/控制字符/擦零及 LCD station 渲染。
- [x] **PASS（工具）** — 错误固件/缺少 challenge 不发送；只发送一次；意外复位不重发；密码行遮蔽并终止；串口读取及清理同时失败仍保留原错误/元数据。
- [x] **PASS（构建/静态）** — 4 配置无 warning/error；clang-format、三个纯逻辑 Clang 分析、11 自有 C 文件加扫描分支共 12 次 GCC -fanalyzer 无诊断。
- [x] **PASS（防持久化）** — Wi-Fi NVS、PHY 校准/分区/多 bin、DHCP last-IP 恢复共 5 开关、US 未确认和非默认 UART0，7 项编译拒绝测试。
- [x] **PASS（镜像）** — 893280 / 0xDA160 字节，SHA-256 `09a63fa3a8b113a31ffa2317edc5803efb3512ac94dee19ad0fde7b4f88d8353`；ESP32、DIO/40 MHz/4 MB、最低 revision 1.0、校验有效。
- [x] **PASS（授权/写入）** — 用户对新 hash/大小/范围及 USB 凭据/DHCP 请求回复“允许，继续”后，仅擦写 **`[0x10000, 0xEB000)`** 一次，49.0 秒、hash 校验通过；不沿用 0.1.3 授权。
- [x] **PASS（启动/凭据/连接）** — 三次正常 RTS 复位、USB 启动令牌/CRC 帧接收并确认，各自 45.00 / 690.15 / 45.11 秒完整采集；均第一次尝试关联并取得 DHCP 地址，US / 1–11 / MANUAL 读回通过，实测信道 9。无凭据值进入公开输出；不以配置支持代替 PMF/SAE 协商验证。
- [x] **PASS（长时）** — 独立 690.1504 秒采集、137 个 online 样本/678.370 秒 online 跨度；137 个 running 样本，心跳 1→685、跨度 683.993 秒；69 个 ILI9341 提交日志，计数 1→681、跨度 680 秒，无异常复位/panic/watchdog/stalled/重连。
- [x] **PASS（内存）** — 124 个启动一分钟后的在线状态样本，总空闲 heap 4388284–4388304、内部 196376–196396 字节；第 2–11 分钟中位数固定为 4388304 / 196396，无持续下降。最低总 heap 4248516；LCD / STA / 状态任务栈低水位 3984 / 5440 / 1664 字节。
- [x] **PASS（PSRAM/LCD）** — 各次启动 PSRAM 启动及 128 KiB 六模式自检通过，外部空闲前后均 4192024；两轮 ILI9341 ID4=93 41，一次已知驱动选择，不试 ST7789。LCD 提交日志持续，不声称新照片或全程人工目视观察。
- [x] **PASS（无凭据复位）** — 独立 135.1956 秒只读采集，UART 无命令/不读取凭据文件。CONFIG 在 120 秒后转 ERROR/ESP_ERR_TIMEOUT，尝试数及 IP 均为零，没有 PHY/网络初始化日志；LCD 提交 1→131、心跳 1→132，继续运行。后续重新提供凭据即恢复在线，不自动沿用旧配置。
- [x] **PASS（保护/收尾）** — 写前实际设备完整 4 MiB 摘要匹配；写后及在线/超时测试后，应用、全部范围外 Flash、8 项安全字段一致；另重建并校验当前完整 4 MiB 预期快照（不是双读）。之后最终 45.11 秒连接采集通过、保持 ONLINE 且 UART 释放；没有声称最终启动后又核验 Flash。
- [ ] **NOT TESTED** — 真实掉线/丢 IP 后退避与恢复、重试耗尽、WPA3/PMF/SAE 协商细节、受控断电恢复；未为此擅自重启路由器或变更主机网络。该历史0.1.4没有HTTP；当前HTTPS状态及OTA另已实测。

方案、构建、凭据通道的物理信任边界和验收计划见 [WIFI_STA.md](WIFI_STA.md)。

### 0.1.6/0.1.7 HTTPS OTA＋LCD固定驱动修正（真实Wi-Fi部署通过，已关闭）

- **PASS** — 用户批准新四hash后USB一次写prefix/seed/A，各1.9/0.1/55.6秒，三hash通过；写前同设备/完整4MiB/8安全字段通过，写后完整预期Flash及8字段通过。
- **PASS** — 0.1.6正常启动180.047秒，PSRAM128KiB六模式、两轮9341、单ILI9341帧1→171、栈最低3992字节；无错误/重启/驱动轮换。真实固定证书TLS＋Bearer GET/status：A、online/healthy/confirmed及运行内容hash匹配。
- **PASS** — 用户确认0.1.6无翻转后，唯一HTTPS POST/200更新0.1.7，实际B写入1014048字节并软件重启，38686ms完成30秒健康确认；无重传或UART写B。客户端完整流程69.1秒，不是纯上传时间。
- **PASS** — 270.037秒过渡采集，B首次心跳0→218、LCD1→221/span220010ms；正常RTS复位B再独立180.024秒，心跳0→179、LCD1→171/span170010ms。两次两轮9341/单ILI9341、PSRAM128KiB六模式、US手动读回、一次嵌入加载/首次关联DHCP、HTTPS及进度均通过，无故障日志；最终认证TLS状态的版本/槽/内容hash/online/healthy/明确VALID匹配。
- **PASS** — Wi-Fi后B及3补集digest、8安全字段、两个控制记录CRC/VALID和完整4MiB当前重建预期匹配；seq1/seq2、A控制扇区不变、选B。核验在最终180秒正常启动前，不谎称之后再查Flash。B写前整槽经离线核实全FF，首次写入新应用；原始Espruino备份保留。

- **PASS** — display_plan 安全规则先预期失败再实现全绿；UNKNOWN/ST/无效ID不生成驱动计划，清空输出，仅ILI9341通过。生产代码无候选轮换/定时重置，两版ELF无ST7789构造符。修正后的四配置构建再次通过。
- **APPROVED/CONSUMED** — 四项hash均已单次执行；A至0x108000、B至0x258000，控制区仅标准SDK状态更新。全部写入许可耗尽，无自动重试/恢复权限。原翻转根因未确认，用户确认bootstrap无翻转，不冒充助手新B全程目视观察。

- **PASS** — 45 项 C ASan/UBSan、15 项 Python、4 个上传 mock 场景；bootstrap/Wi-Fi目标/默认关闭/扫描四构建无 warning/error，C 格式、Babashka REPL/clj-kondo/cljfmt 通过。
- **PASS** — 官方分区生成器验证、唯一 otadata、双等大 OTA 槽、地址/大小保持及 MD5 往返；seq1/VALID seed CRC 与 SDK otatool 一致；TLS key/cert 对应且本地验证通过。
- **NOT TESTED** — 实际失败回退/故障注入、受控断电、新十分钟soak、真实Wi-Fi丢失恢复、WPA3细节、完整8MiB。上传阶段未连续测峰值；B正值状态采样internal最低145764、最终正常启动172924字节，排除初始零占位；不是严格峰值或长时泄漏验收。Wi-Fi上传/重启/新B健康确认已实测通过，见 [OTA.md](OTA.md)。

### 0.1.5-wifi-auto（两次自动连接及保护验证通过）

- 用户明确要求把 SSID/密码编入固件，启动自动连接，不再等待 USB 配网；不增加 NVS 存储或管理接口。含凭据的 bin/ELF/汇编/目标文件仅保存在私有 `.local/`。
- **PASS（软件）** — 44/44 C 主机测试（ASan/UBSan）、11/11 Python 测试；STA+LCD+PSRAM、默认关闭、扫描三配置构建，格式及凭据解析 Clang 分析通过。原 41 项之外增加两行文件解析的有效/无效/长度与 PSK 边界 3 项。
- **PASS（写入/保护）** — 用户对准确 hash/879376 字节/`[0x10000, 0xE7000)` 回复 yes 后，仅写应用一次，48.3 秒、hash 校验通过。写前同一设备及完整 4 MiB 摘要匹配；写后及首轮运行后应用、所有范围外 Flash 和 8 项安全字段匹配，另核验完整当前预期快照。无 bootloader/分区表/NVS/eFuse 修改。
- **PASS（自动连接）** — 两次正常 RTS 复位，独立采集 60.0984 / 60.1013 秒，各短采集判据通过；无任何 UART 凭据输入，无旧 challenge，每次仅加载一次嵌入凭据。均第一次尝试完成关联/DHCP，约 8.233 / 8.223 秒 ONLINE，US/1–11/MANUAL 读回通过。
- **PASS（并发/诊断）** — 两次均有 11 个 online 样本，跨度 46.850 / 46.860 秒；心跳 1→57、LCD 提交 1→51；PSRAM 启动/128 KiB 六模式、两轮 ILI9341 ID 通过，无异常复位/错误/重连。LCD/STA/状态栈低水位 3984/5440/1696 字节。没有新照片或全程目视观察记录。
- **收尾** — 保护核验后最终正常启动并自动 ONLINE，UART 释放、无后台采集；不声称最终启动之后又核验 Flash。**NOT TESTED**：受控断电、本版十分钟长测、真实掉线退避恢复及 WPA3/PMF/SAE 细节。该0.1.5没有SD/HTTP/OTA，详见 [WIFI_AUTO.md](WIFI_AUTO.md)；当前0.1.7已有HTTPS/OTA，SD仍未实现。

## 历史首次Flash备份与0.1.0写入范围

完整记录见 [FLASH_BACKUP.md](FLASH_BACKUP.md)。备份位于 `.local/backups/pre-first-flash-2026-11-07/`，两份 SHA-256 均为 `f2dfaf6c0d21d05016e136c836310263eb2bc84a8a08f727bb79aab8d8c8834b`。

原固件有 Espruino / 2v04 / v3.1.3-dirty 标记，bootloader 精确版本未知。用户已授权且已完成仅覆盖 factory，实际范围 `[0x10000, 0x3B000)`。原启动链与本次镜像的兼容性已实测通过；全部范围外数据保持写前值。

写前发现仅 NVS 变化 958 字节，已建立并全片校验最新快照，其 SHA-256 为 `786af6bde18766e77c6e5e675405ecdb99298bdf29ea522c61be1521425758fd`。完整授权、快照差异、烧录命令、三次启动与 330 秒串口证据见 [FIRST_FLASH.md](FIRST_FLASH.md)。

## 软件证据

首次 0.1.0 基线镜像（历史记录；当前0.1.7 OTA镜像见本页当前摘要及OTA节）：

- 路径：`build/wrover_radio_node.bin`
- 文件大小：**173328 字节**（约 169.3 KiB）；生成的标准 1 MiB 应用分区余量约 83%。设备实际保留原 1344 KiB factory 槽，本次镜像已在该槽实测启动。
- SHA-256：`00e9608629794e042dca0bab72b928cb90da425ab89f6ac551a287d8edb41c21`
- 生成的 bootloader 与分区表只在磁盘上，未写入设备。

精确复现命令见 [BUILD.md](BUILD.md)，本地日志：

- `.local/evidence/idf-clone.log`、`idf-install.log`、`idf-python-freeze.txt`
- `.local/evidence/idf-build.log`、`idf-build-psram.log`、`idf-size.log`
- `.local/evidence/app-image-info.txt`
- `.local/evidence/host-build.log`、`host-tests.log`（8/8）
- `.local/evidence/node-status-analysis.plist`、`gcc-analysis-0.log`、`gcc-analysis-1.log`
- `.local/evidence/reject-target.log`、`reject-idf.log`

主机编译上游 cJSON 时，Apple SDK 报出 6 条 sprintf deprecated 警告；自有代码没有警告，未屏蔽第三方警告。首轮测试暴露 cJSON 大整数浮点格式化舍入问题，已改为内部生成十进制整数并通过边界回归；不接受外部 raw JSON。

本次应用烧录一次成功；写前校验发现 NVS 差异时曾暂停，建立最新快照后才继续，没有跳过检查。新固件串口日志与采集元数据保存在 `.local/flash/first-flash-2026-11-07/`，摘录见 FIRST_FLASH.md；该历史基线阶段没有 LCD 运行证明，当前 LCD 版的 ID 与工作照片见上方里程碑 2。未执行的项目不标为 PASS。原始证据、备份可能包含设备标识或秘密，不入库。
