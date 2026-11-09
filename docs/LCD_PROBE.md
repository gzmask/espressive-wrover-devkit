# LCD受控候选试验：历史0.1.2-lcd-probe

## 历史 LCD 独立诊断阶段状态

本页记录已完成的 0.1.2 独立诊断。后续0.1.5-wifi-auto历史自动连接/诊断/保护结果见 [WIFI_AUTO.md](WIFI_AUTO.md)，旧版 UART 配网及长时结果见 [WIFI_STA.md](WIFI_STA.md)，历史扫描版见 [WIFI_SCAN.md](WIFI_SCAN.md)；下文端口和内存数值属于本历史镜像。

**已按新授权仅烧录应用，实际匹配 ILI9341，操作者已提供工作照片并确认白色正常。** 当时interface 1/B为`/dev/cu.usbserial-21301`，不是当前固定端口。两轮独立复位均读到 indexed ID4=`93 41`，只运行 A / ILI9341，不再尝试 ST7789。三次正常 RTS 启动均通过；连续采集 660.16 秒，LCD 提交计数和心跳持续增长、heap 稳定、无异常。写后及运行后均验证镜像、全部授权范围外 Flash 和 8 项安全字段；最后正常复位，设备留在运行态。

2026-11-07 准备时曾缺少 USB 串口，07:44 UTC 恢复后只读核对同一设备和旧应用，再取得本镜像的新范围授权。USB 重连根因仍未知，见 [HARDWARE.md](HARDWARE.md)。

用户明确要求根据已确认的 PCB 型号查找并尝试一两个候选，因此本轮从“等待实物控制器标签”改为**固定接线的受控对照试验**。最初的候选集合不是实物身份确认；本次后续以实际 ID 和照片确认 ILI9341 可用。不得猜测其他引脚、供电或扩大到其他驱动。

## 当前源码与本页历史的区别

本页保留0.1.2诊断实现及历史硬件结果。当前已部署的0.1.7及其0.1.6 bootstrap已移除UNKNOWN双驱动轮换，仅接受两轮一致9341后一次初始化ILI9341；其他/未知ID停止LCD，不能按本页历史步骤再试ST7789。0.1.6修正版已USB安装、180秒日志通过且用户确认无翻转；随后真正Wi-Fi更新0.1.7，B两次启动/单ILI9341/健康确认通过，最终正常启动180秒。用户的周期翻转在一次复位后暂未复现，根因仍未确认，见 [OTA.md](OTA.md)。

## 候选依据

- [乐鑫 esp_wrover_kit BSP 的 Kconfig](https://github.com/espressif/esp-bsp/blob/master/bsp/esp_wrover_kit/Kconfig) 明确写着 WROVER-KIT 出货使用过两种 LCD 控制器，提供 ILI9341 / ST7789 选择，默认 ILI9341。**默认值不是出货比例证据**，只作为先试 A 的依据。
- [锁定 IDF v5.5.6 的官方 SPI LCD 示例](https://github.com/espressif/esp-idf/blob/v5.5.6/examples/peripherals/spi_master/lcd/main/spi_master_example_main.c) 同时支持 ILI9341 / ST7789V，采用 320×240 横屏以及相同的 v4.1 LCD 引脚。
- A 使用组件管理器下载的官方 [`espressif/esp_lcd_ili9341` 2.1.0](https://components.espressif.com/components/espressif/esp_lcd_ili9341/versions/2.1.0/readme)，B 使用 IDF v5.5.6 内置 ST7789 驱动；不引入 Arduino/LVGL。`dependencies.lock` 锁定组件版本与摘要，包括 `cmake_utilities` 0.5.3。
- ST7789V 的 `85 85 52` 默认 ID 与单字节 SPI 读时序来自 [ST7789V datasheet](https://www.displayfuture.com/Display/datasheet/controller/ST7789V.pdf)，第 8.4.5、9.1.46–48 节。ILI9341 的 D9 索引 / D3 读取方式参考 [spi_lcd_read 的说明](https://github.com/ropg/spi_lcd_read)；仅借鉴读协议，没有引入该 Arduino 库。

资料下载副本在 `.local/evidence/lcd/`，摘要见 `references.sha256`。资料支持候选集合，**不证明本板实际装的是哪一种**，也不确定整块 LCD 模组料号。

## 固定接线和配置

仅允许照片确认的 ESP-WROVER-KIT v4.1：RESET=18、SCLK=19、D/C=21、CS=22、MOSI=23、MISO=25、背光=5（官方定义低电平亮）。详情见 [HARDWARE.md](HARDWARE.md)。不初始化 microSD、NVS、Wi-Fi 或 HTTP，不操作 PSRAM/Flash/JTAG 引脚。

- 默认 `BOARD_DISPLAY_ENABLE=n`；开启还须显式确认 `BOARD_WROVER_KIT_V41_CONFIRMED=y`，否则编译报错。这个确认仅针对 PCB 和接线，**不是控制器身份确认**。
- 以 1 MHz、SPI mode 0 读取 ID；显示降速使用 10 MHz、RGB565 大端、BGR、320×240 横屏候选。使用官方驱动的默认初始化；当时的工作照片已核对未镜像的横屏布局、完整边框和基本色序。白色由操作者另行确认，照片偏青不作为改色序的理由。
- GPIO 配置前预置背光关闭；复位低 20 ms、高 120 ms。首帧传输完成后才开背光。
- 采用内部 DMA 缓冲区 `320×16×2 = 10240` 字节，而非 PSRAM framebuffer。每条带等待 ISR 完成信号后才能复用缓冲区，等待上限 2 秒；DMA 完成不明时关闭背光并保留相关资源直到复位，不释放仍可能被使用的内存，也不自动重试。
- LCD 独占任务优先级 1、栈 6144 **字节**，约 1 Hz 更新；原心跳/状态任务保持优先级 2、各 4096 字节，不共享 LCD 句柄。LCD 初始化失败不停止心跳。
- 诊断配置沿用已验证的 PSRAM 40 MHz、显式分配、revision 1 cache workaround / MEMW；启动测试和应用 128 KiB 自检保留。新镜像已独立通过启动测试和 128 KiB 自检，不沿用旧版 PASS；整个 8 MiB 的全面测试仍未完成。

## 历史身份识别与A/B观察（当前禁止轮换）

每次启动执行两轮独立硬复位及读取：

1. `DA/DB/DC` 分别单字节读取，记录 RDID。
2. `D9=12/13` 选择 `D3` 的第 2/3 字节，记录 ILI9341 的 ID4。
3. 两轮读数完全一致，且仅匹配一个已知签名时，选对应驱动：ILI9341 的 ID4=`93 41`，或 ST7789V 的 RDID=`85 85 52`。
4. 全 `00`、全 `FF`、未知、不稳定或冲突结果均为 UNKNOWN。**不采用官方示例“零则 ILI，否则 ST”的启发式。**

签名明确时只使用对应驱动，持续刷新且不周期性复位面板。否则循环执行 A（ILI9341）60 帧 / 约 60 秒 → B（ST7789）60 帧 / 约 60 秒；切换前关闭背光并硬复位，不增加其他候选。

画面包含：`WROVER RADIO NODE`、`A: ILI9341` 或 `B: ST7789`、`TRIAL` 或 `ID MATCH`、版本、uptime、heap、`WIFI: OFF IP: --`；四条色块应从左到右为红、绿、蓝、白。白边框及 `TL` / `BR` 用于检查完整范围和方向，底部白色方块每秒移动。

**SPI 没有面板应答验证，`ESP_OK` 和帧计数增长都不等于像素可见。** 串口会明确提示需要操作者照片/视频。即使两个候选都能显示，也不能仅凭显示效果断言芯片精确型号。

## 历史构建与主机测试

下述命令只适用于对应历史源码/配置，不能在当前源码上只改PROJECT_VER复现0.1.2。当前构建见 [BUILD.md](BUILD.md) 和 [OTA.md](OTA.md)，不得重新试ST7789。

仓库根目录：

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-lcd-probe \
  -DSDKCONFIG="$PWD/.local/sdkconfig.lcd-probe" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults;$PWD/tests/sdkconfig.lcd.defaults" \
  -DPROJECT_VER=0.1.2-lcd-probe -DIDF_TARGET=esp32 build

cmake -S tests -B .local/host-build -G "Unix Makefiles"
cmake --build .local/host-build --parallel 4
ctest --test-dir .local/host-build --output-on-failure
```

软件验证已通过：

- **20/20 主机测试，ASan/UBSan 开启**：原状态/JSON 8 项、内存模式 4 项、LCD 8 项。LCD 覆盖签名/空值/全零/全 FF、未知/不稳定/冲突、候选计划、RGB565 字节序、边界和保护字、动态标记、文字/候选标签及各条带与逐行渲染一致性。
- 三种配置编译通过且无 warning/error：LCD+PSRAM、LCD 无 PSRAM、默认 LCD/PSRAM 都关闭。
- clang-format、显示逻辑 Clang 静态分析、六个自有 C 文件的 Xtensa GCC `-fanalyzer` 均无诊断。
- 已验证启用 LCD 但未确认 PCB 时的编译保护（对真实构建命令强制覆盖确认宏，预期失败）。
- 镜像头/校验和/内嵌 hash 有效，目标 ESP32、DIO/40 MHz、Flash 4 MB、最低 revision 1.0、IDF v5.5.6、版本 `0.1.2-lcd-probe`；真实编译响应文件包含两个 PSRAM workaround 参数。

日志位于 `.local/evidence/lcd/`：`build.log`、`build-disabled-check.log`、`build-no-psram-check.log`、`host-tests.log`、`display-logic-analysis.plist`、`gcc-analysis-{0..5}.log`、`reject-unconfirmed.log`、`image-info.txt`。这些是软件证据，**不是实机验证**。

## 已授权并部署的镜像与写入边界

- 构建产物：`.local/build-lcd-probe/wrover_radio_node.bin`
- 冻结只读副本：`.local/flash/lcd-probe-2026-11-07/candidate-app.bin`
- 大小：**273792 / 0x42D80 字节**
- SHA-256：`8b9c24dd2fbbce9eb899588c88864363c5dd4f732aa87d4b3219b104c3bab67f`
- 应用起点：`0x10000`；镜像终点不含 `0x52D80`
- 4 KiB 对齐的实际擦写范围：**`[0x10000, 0x53000)`**，共 67 扇区，在原 factory 槽内

此前 `[0x10000, 0x46000)` 的授权没有沿用。助手明确请求本镜像及新范围后，用户回复 **“继续”**，才执行本次写入；记录在 `authorization.txt`。一次写入成功，工具报告实际擦除 `0x10000..0x52FFF`，273792 字节用时 12.9 秒，数据 hash 校验通过。没有写 bootloader、分区表或范围外数据，没有整片擦除、恢复备份或 eFuse 操作，也没有使用 `idf.py flash` / `@flash_args`。此授权不外推到其他镜像或范围。

写前核对端口、镜像、设备与安全字段，并用上次已验证的 0.1.0 整片快照、0.1.1 镜像和已知擦除填充重建当前 4 MB 状态。**整片设备摘要校验匹配后才写入**，不是重新双读整片。快照为 `.local/flash/lcd-probe-2026-11-07/preflash-0.1.1-snapshot.bin`，SHA-256 `ed93691696c81a8d068ee7aabaa0ac47d26228f7d9e01df055343e7cac3e95fb`。

实际执行的应用专用命令（历史记录，不是未来写入授权）：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-21301 --baud 115200 \\
  --before no_reset --after no_reset write_flash --no-progress \\
  --flash_mode keep --flash_freq keep --flash_size keep \\
  0x10000 .local/flash/lcd-probe-2026-11-07/candidate-app.bin
```

写后、首次运行前已通过摘要校验：前缀 `[0, 0x10000)`、获准镜像 `[0x10000, 0x52D80)`、后缀 `[0x53000, 0x400000)`。这覆盖全部授权范围外 Flash。只读比较的 `FLASH_CRYPT_CNT`、`ABS_DONE_0/1`、`DISABLE_DL_*`、`UART_DOWNLOAD_DIS`、`JTAG_DISABLE` 均与写前一致。660 秒运行后再次执行相同的三段摘要校验，全部匹配；8 项安全字段与写前、写后完全一致。随后正常 RTS 复位并采集第三次启动，应用继续正常运行，没有再次烧录。

## 实机验收与照片

- [x] **PASS** — 三次正常 RTS 复位均经原 bootloader 启动新镜像，PSRAM 启动/128 KiB 自检均通过；每次两轮 LCD ID 均为 `RDID=00 00 00 ID4(indexed)=93 41`，匹配 ILI9341。
- [x] **PASS（照片 + 操作者）** — 320×240 配置下，横屏文字可读、无镜像、四边框完整，TL/BR 正确；R/G/B/W 顺序正确，背光已工作。用户确认白色正常，照片偏青是拍摄问题；这不是色度校准。
- [x] **PASS（写后及运行后）** — 新镜像、全部范围外 Flash 和已读取的 8 项安全字段均与预期一致。
- [x] **PASS（串口并发）** — 同一 ILI9341 驱动连续采集 660.16 秒，LCD 提交日志跨度 649.99 秒、计数 1→651；心跳样本跨度 655 秒、计数 1→656；没有切换候选、额外复位、panic/watchdog/stalled、队列丢样或 heap 持续下降。

证据边界：照片证明拍摄时的物理画面，连续提交及调度稳定性由串口日志支持；没有连续视频，不声称操作者目视观察了全部十分钟。

### 串口稳定性与保护证据

| 采集 | 时长 | 心跳 | ID / PSRAM / 错误检查 |
| --- | --- | --- | --- |
| `boot-1` | 35.15 秒 | 1→31 | ILI9341；自检通过；无错误 |
| `boot-2-soak` | 660.16 秒 | 1→656 | 同一驱动持续运行；自检通过；无错误 |
| `boot-3` | 35.16 秒 | 1→31 | 运行后校验后正常启动；自检通过；无错误 |

持续采集中，稳定总空闲 heap 为 **4473280 字节**、内部空闲 **281144 字节**、最低总空闲 **4337840 字节**；LCD 任务栈低水位 **4448 字节**，状态任务 **1824 字节**。外部空闲自检前后均为 **4192136 字节**。识别 8 MiB、映射 4 MiB 不代表整个 8 MiB 已全面验证。

完整证据在 `.local/flash/lcd-probe-2026-11-07/`：

- `authorization.txt`、`candidate.json`、`preflash-full-verify.log`、`app-write.log`
- `postflash-range-verify.log`、`post-soak-range-verify.log`
- `security-before.json`、`security-after.json`、`security-post-soak.json`
- `boot-{1,2-soak,3}.log/.json`、`boots-summary.json`、`soak-summary.json`
- `host-tests-final.log`：20/20，ASan/UBSan；clang-format 复核通过
- `operator-observation.txt`：用户照片及原话

### 照片

照片原件：`.local/evidence/lcd/display-20261107-075746-original.jpg`，来自用户提供的 `PXL_20261107_075746414.jpg`，SHA-256 `ddd485d139486012d92bba5dbff7aa857f9fa11d1897df0136e510e9b404d0c0`。相邻的 `display-20261107-075746-landscape.jpg` 仅为旋转后的查看副本。照片上可读 `A: ILI9341 ID MATCH`、`0.1.2-LCD-PROBE`、`UPTIME: 26 S`、`HEAP: 4473280 B`、`WIFI: OFF IP: --`。

用户先反馈 “I saw the display working”，随后对照片白色偏青澄清“没有，照片问题。挺白的”。原始图片和观察记录只留在 Git 忽略的权限受限目录，不公开照片元数据。

观察时不要带电拆屏。两种候选都无可见画面时先保留串口证据，再检查断电后的连接/面板资料，不扩展 GPIO 或电压猜测。本历史镜像不初始化 Wi-Fi/HTTP，也不代表网络与 LCD 的并发验收通过。后续被动扫描版已另行获准部署并独立完成 690 秒扫描/LCD/心跳并发及保护复核，见 [WIFI_SCAN.md](WIFI_SCAN.md)。
