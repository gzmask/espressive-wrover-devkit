# 首次烧录前：Flash备份与旧布局（历史）

本页记录迁移前Espruino双读备份及旧分区表，不是当前设备布局。当前0.1.7/B/VALID的双槽布局、范围保护和重建快照见 [OTA.md](OTA.md)。备份恢复从未实测，也没有新恢复授权。

检查日期：2026-11-07 UTC。本文件记录**首次烧录前的备份阶段**，该阶段没有执行 Flash 写入、擦除或 eFuse 修改。随后用户授权了仅覆盖 factory 应用，实际烧录及新的写前快照见 [FIRST_FLASH.md](FIRST_FLASH.md)。

## 完整备份

本地目录：`.local/backups/pre-first-flash-2026-11-07/`，目录权限 0700，备份权限 0600，并由 `.gitignore` 的 `.local/` 规则排除。备份可能包含凭据和用户程序，不得上传或提交。

| 文件 | 大小 | 结果 |
| --- | --- | --- |
| `flash-115200-read-1.bin` | 4,194,304 字节 | 115200 baud，392.7 秒 |
| `flash-115200-read-2.bin` | 4,194,304 字节 | 115200 baud，384.0 秒 |

两次读取间保持下载模式，不运行原应用。`cmp` 逐字节比较一致，两份 SHA-256 均为：

```text
f2dfaf6c0d21d05016e136c836310263eb2bc84a8a08f727bb79aab8d8c8834b
```

校验清单位于同目录 `SHA256SUMS`。这是同一主机上的双读校验，不是异地备份；恢复写入尚未实测。

最初 460800 baud 读取报 `Corrupt data, expected 0x1000 bytes but received 0xfdf bytes`，没有生成可用完整备份。保留错误日志 `read-1.log`，不将失败尝试用于恢复。后续使用 115200 完成双读；高速错误的硬件/驱动根因尚未定位，首次写入也应使用 115200。

## 工具和精确命令

本次使用已安装的 ESP-IDF Python 环境：esptool / espefuse **4.13.0**。先前独立 uv 环境的 espefuse 5.1.0 入口在导入 BitStream 时失败，尚未连接硬件；因此改用已验证可用的 IDF 配套工具，不修改全局环境。

安全字段仅读取，不读取/打印密钥块：

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
python -m espefuse --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 \
  summary FLASH_CRYPT_CNT ABS_DONE_0 ABS_DONE_1 \
  DISABLE_DL_ENCRYPT DISABLE_DL_DECRYPT DISABLE_DL_CACHE \
  UART_DOWNLOAD_DIS JTAG_DISABLE --format json \
  --file .local/backups/pre-first-flash-2026-11-07/security-summary.json
```

成功备份命令（仅作为本次操作记录；不要覆盖已有备份文件）：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 \
  --before default_reset --after no_reset read_flash --no-progress 0x0 0x400000 \
  .local/backups/pre-first-flash-2026-11-07/flash-115200-read-1.bin
python -m esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 \
  --before no_reset --after hard_reset read_flash --no-progress 0x0 0x400000 \
  .local/backups/pre-first-flash-2026-11-07/flash-115200-read-2.bin
cmp .local/backups/pre-first-flash-2026-11-07/flash-115200-read-1.bin \
    .local/backups/pre-first-flash-2026-11-07/flash-115200-read-2.bin
```

完整读取使用工具默认 RAM stub；只将临时读写辅助程序上传到 RAM，没有向它发出 Flash 写入/擦除命令。此前初检的 `--no-stub` 记录仍然准确，但不适用于本次完整备份操作。

## 安全配置实测

| 字段 | 值 / 解释 |
| --- | --- |
| `FLASH_CRYPT_CNT` | 0，Flash 加密未启用 |
| `ABS_DONE_0` / `ABS_DONE_1` | 均 false，Secure Boot 未启用 |
| `DISABLE_DL_ENCRYPT` / `DISABLE_DL_DECRYPT` / `DISABLE_DL_CACHE` | 均 false |
| `JTAG_DISABLE` | false；不等于 JTAG 已实测工作 |
| `UART_DOWNLOAD_DIS` | false；该位仅适用于 revision 3 及以后，本板为 revision 1，不作为恢复能力依据 |

UART 恢复能力以实际 ROM 通信成功为依据。没有修改这些字段。

## 原始备份中的实际分区表（迁移前）

从整片备份按 4 KiB 边界搜索分区表标记，仅发现 `0x8000`。提取的表有 8 项，**MD5 校验通过**，非随机套用默认偏移。

| 名称 | 类型 / 子类型 | 起始偏移 | 大小 | 结束偏移（不含） |
| --- | --- | --- | --- | --- |
| nvs | data / nvs | 0x9000 | 12 KiB | 0xC000 |
| otadata | data / ota | 0xC000 | 8 KiB | 0xE000 |
| free | data / 0x40 | 0xE000 | 8 KiB | 0x10000 |
| factory | app / factory | 0x10000 | 1344 KiB | 0x160000 |
| ota_0 | app / ota_0 | 0x160000 | 1344 KiB | 0x2B0000 |
| flash | data / 0x40 | 0x2B0000 | 64 KiB | 0x2C0000 |
| js_code | data / ota | 0x2C0000 | 1 MiB | 0x3C0000 |
| storage | data / ota | 0x3C0000 | 256 KiB | 0x400000 |

现代 ESP-IDF 分区校验器会因 `otadata`、`js_code`、`storage` 都使用 data/ota 而拒绝此表。这是旧布局的语义兼容性问题，**不是 MD5 损坏或备份读取错误**。

离线显示表时使用 `gen_esp32part.py --no-verify` 跳过现代布局语义检查；其二进制读取阶段仍验证 MD5，另用独立 MD5 计算交叉确认。没有因此更改设备，不能把该 CSV 直接作为现代分区配置。

## 原固件与复位确认

- `0x1000` 的原 bootloader 和 `0x10000` 的 factory 应用均通过 esptool 镜像 checksum/hash 校验。
- factory 应用包含 `Espruino`、`2v04`、`v3.1.3-dirty` 版本标记；不是由 `>` 提示符单独猜测。**bootloader 本身的精确 IDF 版本仍未确认**。
- `ota_0` 整个槽及真正的 `otadata` 区域均为 0xFF。factory 是唯一包含有效镜像的应用槽。
- 原应用镜像头写有 1 MB，这是旧应用头字段，不是实测芯片容量；实际 Flash 和 bootloader 头均为 4 MB。
- 备份后正常复位，15 秒采集得到 329 字节，原 ROM 启动日志和 `>` 提示符再次出现；没有发送 REPL 命令。日志为 `post-backup-boot-15s.bin`。这不等于已验证 LCD 或原应用全部功能。

## 当前OTA迁移说明

下述仅应用授权及原表是历史。用户后续另行批准四项准确hash/范围，已安装双槽bootloader/表/seed/A并真正通过HTTPS一次上传0.1.7 B；当前新布局/完整保护验收见 [OTA.md](OTA.md)。旧完整双备份不等于当前设备布局，恢复需要新明确授权，不自动执行。当前重建且实际完整摘要匹配的4MiB快照在 `.local/flash/ota-2026-11-08-lcd-fixed/current-0.1.7-expected.bin`，不是新双读备份。

## 当时的候选方案：只覆盖 factory 应用（后续已获准并完成）

当时0.1.0方案优先保留原bootloader、分区表及数据区；这不是当前仍保留旧引导/表的声明。后续OTA迁移是单独批准的阶段。

本次新镜像：

- 文件：`build/wrover_radio_node.bin`
- 大小：173328 字节 / `0x2A510`
- SHA-256：`00e9608629794e042dca0bab72b928cb90da425ab89f6ac551a287d8edb41c21`
- 候选写入起点：**0x10000**
- 镜像结束偏移（不含）：**0x3A510**
- 正常烧录会擦除并重写的 4 KiB 扇区范围：**[0x10000, 0x3B000)**，完全位于原 factory 槽内。

已经完成的静态核对：目标 ESP32 / 最低 revision 1.0、Flash 4 MB / DIO / 40 MHz、分区表偏移 0x8000 与 MD5 支持匹配；新应用没有启用旧 bootloader 不支持的 SRAM1-as-IRAM 选项。官方说明旧 ESP-IDF bootloader 设计上支持较新应用，参见 [bootloader 兼容性](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-guides/bootloader.html#bootloader-compatibility)。

**备份阶段仅完成静态核对；后续这份具体镜像已通过三次正常启动及超过 5 分钟实机观察**，见 [FIRST_FLASH.md](FIRST_FLASH.md)。该0.1.0历史应用不初始化NVS/Wi-Fi/OTA；后续OTA阶段已重新评估并迁移重复子类型，仍不初始化或擦写旧NVS。不要执行通用 `idf.py flash`，也不要替换原分区表或通过关闭 MD5 检查掩盖问题。

首次覆盖前已明确告知原 Espruino 应用将被替换、原 JS/数据区保留但不再由新固件执行，用户明确同意后才实施。该授权不包含修改 bootloader/分区表、整片擦除或自动备份恢复。若未来启动失败，应停止并评估，不自行扩大写入范围。
