# 首次应用烧录与 FreeRTOS 实机验证

日期：2026-11-07 UTC。本文件是 **0.1.0首次烧录的历史记录**；当前设备已通过真实Wi-Fi部署0.1.7/B/VALID，当前分区/入口/验收见 [OTA.md](OTA.md)。下述“本次”、端口及仅写应用边界只指0.1.0，不能复用其命令/许可。0.1.1 PSRAM历史见 [PSRAM_PROBE.md](PSRAM_PROBE.md)。用户在被明确告知范围及原 Espruino 应用会被替换后，回复 **“同意，继续”**，授权本次仅覆盖 factory 应用。

## 授权和实际写入

| 项目 | 实际值 |
| --- | --- |
| 板卡 / 芯片 | ESP-WROVER-KIT v4.1 / ESP32-D0WD revision 1.0 |
| UART | `/dev/cu.usbserial-21401`，115200 baud |
| 工具 | ESP-IDF 环境 esptool 4.13.0 |
| 应用 | wrover_radio_node 0.1.0 / ESP-IDF v5.5.6 |
| 原始构建产物 | `build/wrover_radio_node.bin` |
| 写入文件 | `.local/flash/first-flash-2026-11-07/approved-app.bin`，经哈希核对的只读副本 |
| SHA-256 | `00e9608629794e042dca0bab72b928cb90da425ab89f6ac551a287d8edb41c21` |
| 长度 | 173328 字节 / 0x2A510 |
| 写入起点 | 0x10000 |
| 实际扇区擦写范围 | **[0x10000, 0x3B000)** |
| 写入次数 | 1 次，成功；工具报告 8.9 秒、数据哈希校验通过 |

没有执行整片擦除、eFuse 修改、Flash 加密/安全启动启用、bootloader 或分区表写入。没有恢复旧镜像，也没有写入 PSRAM 测试版本。

## 写前快照变化与处置

首次写前的全片摘要校验发现设备与早先备份不一致，因此**暂停写入**并定位差异：

- 仅原 NVS 区域发生变化：958 字节，地址跨度 `0xA02C..0xAC45`。
- bootloader、分区表、otadata、原应用以及其余区域均与早先备份一致。
- 这些差异在烧录前已存在；具体触发 NVS 更新的原因未定位，不能把它归因于本次烧录。
- 在下载模式下连续读取前 64 KiB 两次，逐字节一致；其余区间通过设备摘要校验。
- 用最新前缀和已验证的原备份剩余部分重建当前 4 MB 快照，再次通过整片设备摘要校验后，才执行写入。

最新写前快照：`.local/flash/first-flash-2026-11-07/preflash-current-snapshot.bin`。

```text
SHA-256: 786af6bde18766e77c6e5e675405ecdb99298bdf29ea522c61be1521425758fd
```

早先的两份完整读出仍原样保留，见 [FLASH_BACKUP.md](FLASH_BACKUP.md)。最新快照和早先备份均可能含凭据，不得入库或上传。

## 精确烧录命令（已执行记录，不是新的写入授权）

写前已通过整片验证并保持在下载模式，因此使用 `--before no_reset`。没有绕过工具的安全/兼容性检查，没有使用 force 或 erase-all。

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 \
  --before no_reset --after no_reset write_flash --no-progress \
  --flash_mode keep --flash_freq keep --flash_size keep \
  0x10000 .local/flash/first-flash-2026-11-07/approved-app.bin
```

工具的真实输出摘录：

```text
Flash will be erased from 0x00010000 to 0x0003afff...
Compressed 173328 bytes to 97160...
Wrote 173328 bytes (97160 compressed) at 0x00010000 in 8.9 seconds (effective 155.6 kbit/s)...
Hash of data verified.
```

## 写后保护校验

写后、首次运行前，以及长时间运行后，分别使用 `verify_flash --diff no` 校验：

| 区域 | 基准 | 结果 |
| --- | --- | --- |
| `[0x0, 0x10000)` | 最新前缀双读结果 | 两次设备摘要校验均通过 |
| `[0x10000, 0x3A510)` | 获准的新应用镜像 | 两次设备摘要校验均通过 |
| `[0x3B000, 0x400000)` | 原始备份后缀 | 两次设备摘要校验均通过 |

这覆盖了**全部授权擦写范围之外的 Flash**，包括原 bootloader、分区表、最新 NVS、OTA 区和 JS/存储数据区。最后扇区的镜像尾部填充属于获准擦写范围，不声称它保持旧字节。

读取的安全字段 `FLASH_CRYPT_CNT`、`ABS_DONE_0/1`、`DISABLE_DL_*`、`UART_DOWNLOAD_DIS`、`JTAG_DISABLE` 在写前、写后和运行后完全一致；未读取或输出密钥块。

## 三次正常复位与持续运行

这些测试通过 UART/RTS 复位，不是 USB 断电重连，也不是手动 BOOT + EN 恢复操作。

| 运行 | 开始时间 UTC | 采集时长 | 结果 |
| --- | --- | --- | --- |
| 第一次启动 | 06:16:14 | 25.18 秒 | 固件版本正确，队列工作，无错误 |
| 第二次启动 / 持续运行 | 06:18:09 | **330.15 秒** | 超过 5 分钟，单次启动，无意外复位 |
| 第三次启动 | 06:24:17 | 25.16 秒 | 再次正常启动，最终保持新固件运行 |

持续运行日志分析：

- 67 条状态记录，其中 66 条为 running，初态为 starting。
- 有效运行样本跨度 **325,993 ms**。
- 心跳接收计数从 **1 增至 327**，时间戳与计数严格递增。
- 最大状态日志间隔 5993 ms（首次间隔受 5 秒门限与采样相位影响），后续约 5 秒。
- 10 秒后的空闲 heap 始终为 **300,224 字节**；运行期最低 heap 为 **295,164 字节**。
- 状态任务栈低水位 **1832 字节**，配置栈大小为 4096 字节。
- 没有 stalled 状态、错误码、panic、abort、watchdog、brownout 或额外 ROM 启动标记。
- 实机报告 CPU 为 **160 MHz**、Flash **4,194,304 字节**；PSRAM 明确标为 NOT TESTED。

## 真实串口摘录

以下摘自实机日志，不是模拟输出：

```text
I (190) radio_node: WROVER RADIO NODE firmware=0.1.0 idf=v5.5.6 target=esp32
I (200) radio_node: detected flash=4194304 bytes
I (220) radio_node: PSRAM not configured: presence and size NOT TESTED
I (220) radio_node: LCD not initialized; Wi-Fi/HTTP disabled; no application GPIO writes
I (250) radio_node: two FreeRTOS tasks started; heartbeat=1 Hz; status log=5 s; queue=4
I (250) radio_node: status={"firmware":"0.1.0","state":"running","uptime_ms":74,"heartbeat_count":1,"free_heap_bytes":295768,"min_free_heap_bytes":295752,"free_internal_heap_bytes":295768,"last_error":null}
I (326250) radio_node: status={"firmware":"0.1.0","state":"running","uptime_ms":326067,"heartbeat_count":327,"free_heap_bytes":300224,"min_free_heap_bytes":295164,"free_internal_heap_bytes":300224,"last_error":null}
I (326250) radio_node: status task stack low-water=1832 bytes
```

## 证据与剩余范围

完整本地证据目录：`.local/flash/first-flash-2026-11-07/`。

- `authorization.txt`、`approved-app.bin`、`app-write.log`
- `preflash-full-verify.log`（首次不匹配）、`preflash-range-verify.log`
- `prefix-current-read-1.bin`、`prefix-current-read-2.bin`、`preflash-current-snapshot.bin`
- `preflash-current-full-verify.log`、`PREFLASH_SHA256SUMS`
- `postflash-range-verify.log`、`post-soak-range-verify.log`
- `security-before.json`、`security-after.json`、`security-post-soak.json`
- `boot-1.log`、`boot-2-soak.log`、`boot-3.log` 及各自采集元数据 JSON
- `boots-summary.json`、`soak-summary.json`、`host-tests.log`

使用 pyserial 控制 UART/RTS，采集期间没有发送 UART 命令。另复跑 8 项主机测试和 clang-format，均通过，固件源码及获准镜像未改变。

**历史结论：当时无PSRAM的0.1.0基础固件通过首次烧录、三次正常复位和至少 5 分钟持续运行验证。** 原启动链与这份具体镜像的兼容性得到实机证实，不自动外推到未来版本。

在本次 0.1.0 验证结束时，PSRAM 实际容量与使用仍为 NOT TESTED；后续已由独立诊断版完成容量识别和有限读写测试，见 PSRAM_PROBE.md。LCD 控制器/显示、Wi-Fi、HTTP、USB 断电重连、手动 BOOT + EN、备份恢复写入仍未测试。该历史阶段旧表的多data/ota问题未因启动成功被修复；后续0.1.6已按新授权迁移双槽表，当前状态不能沿用此结论。
