# 硬件盘点

初检：2026-11-07 01:14 UTC；当前状态复核：2026-11-08。硬件为同一ESP-WROVER-KIT v4.1，当前固件0.1.7/B/VALID已通过真实Wi-Fi部署，见 [OTA.md](OTA.md)。下方早期ROM日志/命令均属历史，不代表设备仍运行Espruino；串口名每次重插须重新发现。

## 实测与待确认项

| 项目 | 结果 | 依据 / 限制 |
| --- | --- | --- |
| 开发板 | **ESP-WROVER-KIT v4.1** | 用户提供的 LCD 面照片右上角清晰显示 ESP-WROVER-KIT_V4.1 |
| 模组 | ESP32-WROVER-B（交接资料描述） | 未在本次检查中核对实物标记；芯片探测不能确认模组型号 |
| 芯片 | ESP32-D0WD，revision v1.0 | esptool ROM 通信 |
| 目标 | esp32 | 不是 esp32s3 |
| CPU | 双核；当前固件运行于 **160 MHz**，芯片支持 240 MHz | 160 MHz 由新固件启动日志实测确认 |
| 晶振 | 40 MHz | esptool |
| Flash | 4 MB；Manufacturer 0x20，Device 0x4016 | flash-id；后续两次完整读取 4,194,304 字节，逐字节一致 |
| Flash 电压 | 工具报告 strapping pin 选择 3.3 V | 未手动改变供电/strap 设置 |
| PSRAM | **8 MiB 器件，4 MiB 映射入 allocator** | 40 MHz，CLK=GPIO17 / CS=GPIO16；三次启动及 128 KiB 六模式自检通过，见 PSRAM_PROBE.md |
| USB 桥 | FTDI Dual RS232-HS，VID:PID 0403:6010 | macOS ioreg，符合 FT2232H 系列双通道桥 |
| USB interface 0 / A | 最后为 `/dev/cu.usbserial-100` | 最新枚举；官方映射为JTAG，未验收JTAG调试/跳线，名称不可固定 |
| USB interface 1 / B | 最后为 `/dev/cu.usbserial-101` | 最新ROM通信和串口采集确认为UART；重插后重新发现 |
| UART | 115200，8N1，无硬件流控 | 启动日志可读，ROM 探测成功 |
| LCD 实物 | 已收到工作照片 | PXL_20261107_075746414.jpg 显示可读诊断内容、完整边框及 RGBW 色条，用户确认白色正常 |
| LCD 控制器/显示 | **ILI9341，320×240 横屏配置工作** | 两轮 indexed ID4=93 41；用户照片显示完整边框、非镜像文字和正确色序，并确认白色正常，见 LCD_PROBE.md |
| LCD 引脚 | 已核对v4.1官方接线并以固定配置运行 | 资料与运行证据，不是独立电气测量 |

官方参考：[ESP-WROVER-KIT v4.1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp-wrover-kit/user_guide.html)、[v4.1 原理图](https://dl.espressif.com/dl/schematics/ESP-WROVER-KIT_V4_1.pdf)。本次已从照片确认对应 PCB 版本。指南说明 FT2232HL A 通道用于 JTAG、B 通道用于 UART。

本地官方文件已集中到 [官方参考资料](references/espressif/README.md)：用户指南 HTML、原理图 PDF、正反面布局 PNG 与 DXF 的来源及校验清单。本机保留原文件，公开仓库不包含这些原文件。该页补充 SD/LCD/JTAG/RGB/摄像头共享引脚与候选玩法；它们是硬件资料，不表示本固件已实现或实测这些新功能。

### v4.1 LCD 接线（已由固定配置运行，未做独立电气测量）

| LCD 信号 | ESP32 GPIO |
| --- | --- |
| RESET | 18 |
| SCL / SPI SCLK | 19 |
| D/C | 21 |
| CS | 22 |
| SDA / MOSI | 23 |
| SDO / MISO | 25 |
| Backlight | 5 |

接线已交叉核对用户指南 LCD/U5 表、v4.1 原理图及[官方 SPI LCD 示例（v5.4.2）](https://github.com/espressif/esp-idf/blob/v5.4.2/examples/peripherals/spi_master/lcd/main/spi_master_example_main.c)。示例使用低电平点亮背光；本次诊断版按此配置，实际工作照片已确认点亮。引用该版本示例不代表已选定项目 ESP-IDF 版本。

该示例同时支持 ILI9341 与 ST7789V，因此 PCB 版本本身不足以确认实际面板控制器。官方 esp_wrover_kit BSP 的 Kconfig 也明确说明出货使用过这两种控制器，默认 ILI9341 不代表出货比例。

应用户要求，已准备固定 v4.1 接线的受控候选诊断版：先做两轮已知 ID 签名读取，读数不明确时每 60 秒试 A（ILI9341）/ B（ST7789）；不以全零/全 FF 判断型号，不自动增加其他候选。后续另行取得新范围授权并部署 0.1.2-lcd-probe，两轮 ID4=93 41 匹配 ILI9341，用户提供工作照片且确认白色正常；未试 ST7789。方案、来源、写入记录和剩余项见 [LCD_PROBE.md](LCD_PROBE.md)。上述UNKNOWN轮换仅属历史：当前已部署0.1.7已彻底移除，只接受确认ILI9341，其他/未知ID停止LCD；0.1.6经180秒采集及用户确认无翻转后，已实际通过Wi-Fi部署0.1.7，B两次启动/单ILI9341及HTTPS确认通过。

## 主机与工具

- Darwin 27.0.0，arm64；本次使用 macOS，而非交接资料优先建议的 Linux。
- CMake 3.28.2。
- Python 3.12.10、esptool 5.1.0、pyserial 3.5。
- 前期 esptool 5.1.0 经 uv 隔离工具缓存运行，未安装进全局 Python 环境。
- 后续已安装 **ESP-IDF v5.5.6**，commit `880a203ae603d08a94d06aa01e0d6761958e5e9e`，源码位于 `.local/esp-idf`，工具位于 `.local/espressif`；每个 shell 需显式 export，未修改全局 shell 配置。
- 固件构建使用 Xtensa GCC 14.2.0（esp-14.2.0_20260121）、CMake 3.28.2、Ninja 1.13.0、IDF 环境 esptool 4.13.0。完整版本、安装与验证步骤见 [BUILD.md](BUILD.md)。
- 无 PSRAM 的 0.1.0 基线与 0.1.1-psram-probe 均已分别获准烧录，各通过三次正常复位及约 330 秒持续运行。两版及 0.1.2-lcd-probe 均为历史基线；0.1.3 扫描版也已完成独立验证。0.1.5历史另行获准仅写应用，显式启用 LCD、PSRAM、STA/DHCP，凭据编入应用、启动自动连接；两次无 UART 输入复位及保护复核通过。源码默认关闭可选功能，旧 0.1.4 的 USB/RAM-only 凭据限制只属历史。

## 早期只读ROM探测记录（旧工具/端口，仅历史）

以下5.1.0命令是当时的执行记录，不是当前推荐命令。如今使用IDF环境4.13.0及重新发现的端口，示例见 [FLASH_AND_RECOVERY.md](FLASH_AND_RECOVERY.md)。只读ROM探测也会中断应用，不在文档维护任务中执行。

先使用 ioreg 将设备接口与串口路径对应，并用 lsof 确认没有已有串口使用者，然后执行：

```sh
uv tool run --from esptool==5.1.0 esptool version
uv tool run --from esptool==5.1.0 esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 --no-stub --connect-attempts 3 chip-id
uv tool run --from esptool==5.1.0 esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 --no-stub --connect-attempts 3 flash-id
```

两项探测退出码均为 0。经典 ESP32 没有该命令所指的独立 chip ID，工具提示后改为读取 MAC，属于预期行为。公开文档不记录完整 MAC。

命令会切换 ROM 下载模式并通过 RTS 复位；使用 --no-stub，没有上传 RAM flasher stub，没有执行 Flash 写入/擦除或 eFuse 写入。

## 原有Espruino固件观察（迁移前历史）

先被动读取 UART 8 秒，取得 326 字节；随后正常 RTS 复位并读取 12 秒，取得 329 字节。采集使用 pyserial（Python 的专用串口库），DTR=False，不发送任何 UART 数据。

复位后的原始可读内容：

```text
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x1e (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0018,len:4
load:0x3fff001c,len:2668
load:0x40078000,len:7304
load:0x40080000,len:5312
entry 0x40080274

>
```

能看到 ROM 加载 Flash 镜像并跳转，随后出现 > 提示符。**不能据此确认原有应用类型、版本、PSRAM 状态或整体健康状况**；没有向未知提示符发送命令。随后用户提供了 LCD 面照片，屏幕无可读内容，供电状态不确定；仍没有 LCD 运行证明。后续已完成两份一致的整片备份、只读安全字段与分区核对，并从 factory 镜像发现 Espruino / 2v04 / v3.1.3-dirty 标记；详情见 [FLASH_BACKUP.md](FLASH_BACKUP.md)。bootloader 的精确版本仍未知。

原始证据保存在本机、权限受限且 Git 忽略的目录：

- .local/evidence/uart-passive.bin
- .local/evidence/uart-reset.bin
- .local/evidence/chip-id.txt
- .local/evidence/flash-id.txt
- .local/evidence/board-component-side.png（用户第一张照片）
- .local/evidence/board-lcd-side-v4.1.png（用户第二张照片，PCB 版本依据）
- .local/evidence/ESP-WROVER-KIT_V4_1.pdf（官方原理图）
- .local/evidence/wrover-v4.1-user-guide.html（官方用户指南）
- .local/evidence/official-spi-lcd-example-v5.4.2.c（仅参考，不是项目实现）

## 后续只读备份结论

- Secure Boot 未启用，FLASH_CRYPT_CNT=0；未修改 eFuse。
- 原分区表位于 0x8000，MD5 正确，但多个 data/ota 子类型不符合现代校验器规则，不能直接迁移该表。
- factory 位于 0x10000，1344 KiB；ota_0 和 otadata 为空，factory 是唯一有效应用镜像。
- 460800 baud 完整读取失败，115200 下两份完整备份一致；高速故障根因未定位。
- 备份阶段结束时正常复位，并再次看到原有启动日志及 > 提示符。该阶段没有烧录新固件，也没有向提示符发送命令。
- 备份、校验和及原布局见 [FLASH_BACKUP.md](FLASH_BACKUP.md)。

## 经授权的首次应用烧录（历史 0.1.0 基线）

首次仅覆盖 factory 应用，实际擦写范围 `[0x10000, 0x3B000)`。当时部署 wrover_radio_node 0.1.0 / ESP-IDF v5.5.6，经原 bootloader 启动；三次正常复位与 330 秒连续运行均通过。稳定空闲 heap 为 300224 字节，最低 heap 为 295164 字节。该基线没有启用 PSRAM，LCD/网络未初始化。

写前发现原 NVS 较早先备份变化 958 字节，已双读变化区域、更新快照并通过整片校验后才写入。烧录后和持续运行后，全部授权擦写范围外数据及已检查安全字段均保持一致，详情与真实串口摘录见 [FIRST_FLASH.md](FIRST_FLASH.md)。

## PSRAM 独立实机验证（历史 0.1.1-psram-probe）

用户明确批准扩大范围后，仅在 `[0x10000, 0x46000)` 写入诊断镜像，使用 40 MHz、revision 1 cache workaround、显式外部 RAM 分配。实测器件容量 **8388608 字节**，allocator 容量 **4194304 字节**，外部空闲量 **4192136 字节**。

三次正常复位均通过 ESP-IDF 启动测试和 128 KiB 六模式自检；自检释放前后空闲量一致。330.12 秒连续采集中，心跳 1→326，稳定总空闲 heap 4492480 字节（其中内部 300344 字节），无异常复位、panic、watchdog 或 stalled。写后与运行后，全部授权范围外 Flash 和已检查安全字段均保持一致。

预期的“仅能映射 4 MB”警告是经典 ESP32 地址空间限制，不是内存测试失败。没有启用 bank switching/2T，未验证整个 8 MiB 的所有字节或持续内存压力；详情和真实串口摘录见 [PSRAM_PROBE.md](PSRAM_PROBE.md)。

## USB 重连与只读复查（2026-11-07 07:44 UTC）

此前多次检查没有 FTDI、USB 串口或扩展坞的 USB 总线。本次重新出现 ThinkPad 雷电扩展坞 USB 总线及 FTDI 0403:6010；interface 0/A 对应 `/dev/cu.usbserial-21300`，interface 1/B 对应 `/dev/cu.usbserial-21301`。串口无占用，系统自带 AppleUSBFTDI 驱动已绑定。**USB 消失的根因仍未知，不能因先前缺少枚举便断言线材损坏，也不能因操作过 BOOT 便认定它修复了 FTDI 枚举。** 系统日志读取被权限拒绝，未提权或修改驱动。

使用 IDF 环境 esptool 4.13.0、115200、`--no-stub` 执行 `chip_id` / `flash_id`，均成功；MAC 与早先记录匹配（不在公开文档记录完整值）。芯片仍为 ESP32-D0WD v1.0、晶振 40 MHz、Flash 4 MB。

正常 RTS 复位后采集 18.07 秒，4599 字节：仍为 `0.1.1-psram-probe` / IDF v5.5.6；PSRAM 8 MiB / 映射 4 MiB，启动内存测试和 128 KiB 六模式自检通过，心跳 1→16。仅见预期的 4 MiB 映射限制警告，无 panic/watchdog/stalled。这是短时重连复查，不替代十分钟 LCD 验收。

证据位于权限受限、Git 忽略的 `.local/evidence/usb-reconnect-2026-11-07-0744/`：`chip-id.log`、`flash-id.log`、`normal-boot.log`、`normal-boot.json`、`summary.json`。该次复查没有执行 Flash/eFuse 写入、分区修改或 LCD 初始化，发生在后续 LCD 镜像部署之前，不代表当前部署状态。

## 剩余实物确认

PCB 版本与安装 LCD 的照片已收到，不再要求重复拍摄。模组标签和面板料号仍待确认；用户已要求对官方支持的两个控制器进行受控试验，现已用实际 ID 和可读工作照片验证 ILI9341，不要求为查标签而拆屏，禁止带电拆卸。USB 串口已恢复；LCD 诊断固件现已获准部署，ID 与实际工作照片均验证通过，三次启动、660.16 秒连续并发运行及写后/运行后保护核验均通过。Wi-Fi 扫描版现已获准只写应用一次，操作者确认新画面可见 `AP:8, SCANS:1`、无需照片。原长时采集在约 557 秒因串口消失中断；17:38 UTC 重新发现 FTDI interface A/B 为 `/dev/cu.usbserial-20300` / `/dev/cu.usbserial-20301`，MAC 与原设备相同，8 项安全字段和镜像/全部范围外 Flash 均校验一致，未再次烧录。中断的物理/主机根因尚未确定，不作为受控断电恢复验收。后续同一镜像完整采集 690.14 秒、11 轮扫描通过，轮间 heap 稳定；运行后镜像/全部范围外 Flash/8 项安全字段再次一致，随后正常复位采集 45.17 秒通过并保持运行，见 [WIFI_SCAN.md](WIFI_SCAN.md)。用户后续明确告知所在地为美国。0.1.4-wifi-sta 已另行获准仅写应用一次，US / 1–11 / MANUAL 读回、三次首次尝试关联/DHCP、690.15 秒在线并发、135.20 秒无凭据复位超时及写后/运行后保护均通过。该旧版当时保持 ONLINE、重启需 USB 重新传入凭据；后来0.1.5两次无UART输入自动连接及保护通过，现已真正Wi-Fi更新0.1.7/B/VALID，见 [OTA.md](OTA.md)；0.1.5历史记录见 [WIFI_AUTO.md](WIFI_AUTO.md)。没有新增照片或全程人工观察记录，真实掉线退避恢复和受控断电仍未验收，见 [WIFI_STA.md](WIFI_STA.md)。
