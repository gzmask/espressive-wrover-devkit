# PSRAM探测：历史0.1.1固件与实机结果

本页范围是0.1.1；当前部署为已通过真实Wi-Fi更新的0.1.7/B/VALID，见 [OTA.md](OTA.md)。历史源码/构建命令、端口和仅写factory授权不用于当前设备；只改PROJECT_VER不能复现旧固件。

日期：2026-11-07 UTC。用户明确同意将应用擦写范围扩大为 `[0x10000, 0x46000)` 后，已仅覆盖 factory 应用。本阶段曾运行 **0.1.1-psram-probe**（后续已被 [0.1.2-lcd-probe](LCD_PROBE.md) 替换）；识别 **8 MiB PSRAM**，映射 **4 MiB**，三次启动均通过内置测试和 128 KiB 六模式自检，并完成约 330 秒稳定性观察。旧 0.1.0 镜像及写前快照已保留。

本结果不是完整 8 MiB 的全字节/长时间压力测试；未启用 bank switching，测试范围和限制如下。

## 硬件依据与配置

已确认 PCB 为 ESP-WROVER-KIT v4.1，芯片为经典 ESP32-D0WD revision 1.0。官方 [v4.1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp-wrover-kit/user_guide.html) 指定 PSRAM 使用 **GPIO17 / CLK、GPIO16 / CS**，并与 Flash 共享其余 SPI 数据线；不是试探任意 GPIO。模组标记仍未清晰核对，不能由板号推断实际 PSRAM 容量。

探测配置位于 `tests/sdkconfig.psram.defaults`，只在显式构建诊断版时叠加，不改默认关闭 PSRAM 的配置：

- 自动识别 PSRAM 类型，40 MHz，与既有 40 MHz Flash 配置匹配。
- `CONFIG_SPIRAM_CACHE_WORKAROUND=y`，使用 MEMW 策略。revision 1.0 存在官方记录的 PSRAM cache 缺陷，不能关闭此 workaround。
- 已检查生成的 `toolchain/cflags` 以及四个自有 C 源文件的真实编译命令：编译器响应文件包含 `-mfix-esp32-psram-cache-issue` 和 `-mfix-esp32-psram-cache-strategy=memw`。不是只看 menuconfig 开关。
- `SPIRAM_USE_CAPS_ALLOC`：普通 malloc 默认仍使用内部内存；探测缓冲区明确用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` 分配。
- 保持任务栈、BSS 和 NOINIT 在内部 RAM；不启用 himem/bank switching 或 2T 模式。
- 未改变供电、strapping pin、eFuse、bootloader 或分区表；LCD / Wi-Fi / HTTP 不初始化。

参考：锁定的 ESP-IDF v5.5.6 [外部 RAM 文档](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-guides/external-ram.html) 与其 `components/esp_psram/esp32/Kconfig.spiram`。

## 两层内存测试

1. **ESP-IDF 启动测试**：`CONFIG_SPIRAM_MEMTEST=y`。官方实现于 heap 接管前，对映射区域每 32 字节测试一个 word；这不是覆盖所有字节的完整 RAM 测试，应用也不会危险地再次调用该私有测试函数。
2. **应用自检**：心跳/状态任务已启动后，在 app_main 所在任务中独占分配 **128 KiB** PSRAM。验证指针确实位于外部 RAM，再完成六轮带位置索引的 32 位模式写入/读回；整块大于 32 KiB cache，避免只测到小段缓存。各阶段之间让出 CPU，结束后释放缓冲区。

`components/memory_pattern/` 只访问调用者提供的缓冲区，不知道或操作原始硬件地址。算法为 `((uint32_t)word_index * 0x9e3779b9) ^ seed`；六个 seed 覆盖互补模式和非规则模式。主机测试核对固定已知值、128 KiB 往返/保护字、逐位置损坏与首错索引、空指针/长度边界。

`main/psram_probe.c` 负责分配、位置检查、让出 CPU、日志和释放；分配失败或初始化不可用报告 NOT TESTED，不冒充 PASS。检测到数据错误会记录 FAIL。

### 初始化失败的边界

`SPIRAM_IGNORE_NOTFOUND=y` 允许初始化未成功时继续以内部 RAM 运行，不代表能够证明“没有 PSRAM”。**它不忽略启动内存测试失败**：官方代码在测试发现错误时会 abort，可能导致复位循环。发生这种情况须停止验证并报告，不能自动改电压、试 GPIO、启用 2T 或恢复/重刷旧镜像。

## 容量的含义

经典 ESP32 的普通外部 RAM 地址映射最多约 4 MiB。若模块带有更大的芯片，芯片容量和当前可分配 heap 不能混为一谈。

本版分别记录：

- IDF 的 `Found ...MB PSRAM device` 初始化日志。
- `esp_psram_get_size()` 的设备容量；在此目标、锁定版本和关闭 2T 的条件下，它来自物理容量识别。
- `heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` 和 free size，表示当前加入 allocator 的容量。
- 128 KiB / 六模式自检结果。

即使发现 8 MiB 芯片，也不能声称测试了全部 8 MiB。本阶段不启用高地址 bank switching，也不为记录一个更大的数字降低可靠性。

## 复现构建

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-psram-probe \
  -DSDKCONFIG="$PWD/.local/sdkconfig.psram-probe" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults" \
  -DPROJECT_VER=0.1.1-psram-probe -DIDF_TARGET=esp32 build
```

默认无 PSRAM 路径也已用独立 `.local/build-base-check` / `.local/sdkconfig.base-check` 构建，源码默认版本为 0.1.1。经后续明确授权，诊断镜像已部署；没有覆盖本地保留的 0.1.0 镜像副本，也没有部署默认无 PSRAM 的 0.1.1 构建。

## 构建与软件验证结果

- **PASS** — 有/无 PSRAM 两个构建，无 warning/error。
- **PASS** — 诊断镜像头：ESP32、最低 revision 1.0、4 MB Flash、DIO/40 MHz；checksum/hash valid。
- **PASS** — 原 8 项状态测试 + 新 4 项内存模式测试，共 **12/12**；ASan/UBSan 开启。
- **PASS** — clang-format；内存模式的 Clang 静态分析；四个自有 C 源文件的 Xtensa GCC `-fanalyzer`，无诊断。
- **PASS** — cache workaround 和禁止外部任务栈/外部 BSS 的实际生成配置已核对。
- **PASS** — 实机识别 8,388,608 字节 PSRAM，allocator 总容量 4,194,304 字节，初始/测试后外部空闲量均为 4,192,136 字节。
- **PASS** — 三次正常复位均经原 bootloader 启动，ESP-IDF 启动内存测试和应用六轮自检全部通过；330.12 秒采集中无意外复位、panic、watchdog 或 stalled。
- **NOT TESTED** — 未映射的高 4 MiB、整个器件的全字节测试、长时间连续读写压力测试、USB 断电重连。

本地日志：`.local/evidence/psram-probe-build.log`、`psram-disabled-build.log`、`psram-host-tests.log`、`psram-probe-image-info.txt`、`psram-analysis-*.log`。

## 已批准并完成的应用写入范围

| 项目 | 实际值 |
| --- | --- |
| 固件版本 | **0.1.1-psram-probe** |
| 构建文件 | `.local/build-psram-probe/wrover_radio_node.bin` |
| 冻结的只读副本 | `.local/flash/psram-probe-2026-11-07/candidate-app.bin` |
| SHA-256 | `3e8130cd4b665aa41c3a75a733ad551cf601f08ab2a14127ffc208edff9bb3ef` |
| 文件大小 | **220528 字节 / 0x35D70** |
| 写入起点 | **0x10000** |
| 镜像结束（不含） | **0x45D70** |
| 实际擦写扇区 | **[0x10000, 0x46000)** |
| 上次获准扇区 | [0x10000, 0x3B000) |

比上次多覆盖 11 个 4 KiB 扇区（44 KiB），仍全部位于原 factory 槽 `[0x10000, 0x160000)` 内，不涉及 bootloader、分区表或 NVS/JS/其他数据分区。

用户对明确的新范围回复 **“ok, do it”** 后才进行本次写入。以 115200 baud 成功烧录一次；工具报告 10.3 秒，实际擦写 `0x10000..0x45FFF`，数据哈希校验通过。没有执行通用 `idf.py flash`、整片擦除、bootloader/分区表写入、eFuse 修改、2T 尝试或自动恢复。

## 写前快照与保护校验

本轮先核对候选镜像哈希、串口及安全字段。用上次已验证的写前快照、0.1.0 镜像及已知擦除填充重建当前 4 MB 状态，再通过**整片设备摘要校验**确认，才执行写入。此次不是重新读出两份完整 Flash；早先的双读备份仍保留。

写前的 0.1.0 整片快照：`.local/flash/psram-probe-2026-11-07/preflash-0.1.0-snapshot.bin`。

```text
SHA-256: 2da892f694332363f8ae8333f030358e139e070257920f3e112094f61fe14055
```

实际执行的应用专用命令如下。执行前设备已通过只读校验并停留在下载模式；这是已执行记录，不是对新版本或其他范围的授权：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-21401 --baud 115200 \
  --before no_reset --after no_reset write_flash --no-progress \
  --flash_mode keep --flash_freq keep --flash_size keep \
  0x10000 .local/flash/psram-probe-2026-11-07/candidate-app.bin
```

烧录后、首次运行前和持续运行后，分别校验以下区域，两次均通过设备摘要检查：

| 区间（结束不含） | 校验基准 |
| --- | --- |
| `[0x0, 0x10000)` | 写前快照前缀，含原 bootloader、分区表、NVS 和 otadata |
| `[0x10000, 0x45D70)` | 获准的诊断镜像 |
| `[0x46000, 0x400000)` | 写前快照后缀，含其余应用槽及原 JS/存储分区 |

所有授权擦写范围外的 Flash 均保持写前状态；镜像最后扇区内的填充属于获准范围。读取的 `FLASH_CRYPT_CNT`、`ABS_DONE_0/1`、`DISABLE_DL_*`、`UART_DOWNLOAD_DIS`、`JTAG_DISABLE` 在写前、写后和运行后完全一致；未读取或打印密钥块。

## 实机运行证据

| 运行 | 开始时间 UTC | 时长 | 结果 |
| --- | --- | --- | --- |
| 第一次启动 | 06:50:39 | 30.11 秒 | 内置内存测试 + 六轮应用自检 PASS |
| 第二次启动 / 持续运行 | 06:51:49 | 330.12 秒 | 自检 PASS；单次启动，无意外复位 |
| 第三次启动 | 06:58:13 | 30.11 秒 | 再次自检 PASS，最终保持诊断固件运行 |

以上为 UART/RTS 正常复位，不是 USB 断电重连或手动 BOOT + EN 操作。

持续运行中有 67 条状态记录，其中 66 条为 running，有效运行样本跨度 **325 秒**；计数从 **1 增至 326**，每 5 秒记录一次状态，时间戳与计数严格递增。稳定内存数据：

| 指标 | 实测字节数 |
| --- | --- |
| PSRAM 器件容量 | 8,388,608 |
| PSRAM allocator 总容量 | 4,194,304 |
| PSRAM 自检前 / 释放后空闲 | 4,192,136 / 4,192,136 |
| 稳定内部空闲 heap | 300,344 |
| 稳定总空闲 heap（含 PSRAM） | 4,492,480 |
| 全程最低总空闲 heap（含测试分配） | 4,356,336 |
| 状态任务栈低水位 | 1824 |

没有错误、panic、abort、watchdog、brownout 或 stalled。每次启动仅有一条预期警告：8 MiB 器件无法全部映射，当前映射 4 MiB；这不是测试失败，也没有为消除该警告改变硬件配置。

真实串口摘录：

```text
I (123) esp_psram: Found 8MB PSRAM device
I (123) esp_psram: Speed: 40MHz
W (127) esp_psram: Virtual address not enough for PSRAM, map as much as we can. 4MB is mapped
I (1041) esp_psram: SPI SRAM memory test OK
I (1238) psram_probe: device_size=8388608 bytes allocator_total=4194304 bytes allocator_free=4192136 bytes
I (1248) psram_probe: clock=40 MHz CLK=GPIO17 CS=GPIO16; testing only allocated RAM
I (1298) psram_probe: self-test pass=1/6 seed=00000000 OK
I (1548) psram_probe: self-test pass=6/6 seed=fedcba98 OK
I (1558) psram_probe: self-test PASS: 131072 bytes, 6 patterns; buffer released; external_free=4192136 bytes
```

完整本地证据位于 `.local/flash/psram-probe-2026-11-07/`：`authorization.txt`、`candidate.json`、`app-write.log`、`preflash-full-verify.log`、`postflash-range-verify.log`、`post-soak-range-verify.log`、三份 `security-*.json`、`boot-1.log` / `boot-2-soak.log` / `boot-3.log` 及采集元数据、`boots-summary.json`、`soak-summary.json`、`host-tests.log`。采集工具为已验证的 pyserial 本地辅助程序，没有发送 UART 命令。主机 12 项测试和格式检查再次通过。

本历史 PSRAM 专用固件未初始化 LCD、Wi-Fi 或 HTTP，本节结果不能替代后续版本的验证。后续 LCD 版已独立启动、读取 ID 并收到工作照片，详见 [LCD_PROBE.md](LCD_PROBE.md)；历史 0.1.3-wifi-scan 已独立通过 PSRAM 自检、扫描/LCD/心跳并发及保护复核，见 [WIFI_SCAN.md](WIFI_SCAN.md)。旧 0.1.4-wifi-sta 的 PSRAM 自检、STA/DHCP、690 秒并发和保护已独立通过，见 [WIFI_STA.md](WIFI_STA.md)。0.1.5历史两次自检/自动连接/保护通过；当前已实际Wi-Fi部署0.1.7，B两次PSRAM自检及HTTPS确认通过，见 [OTA.md](OTA.md)；旧自动连接结果见 [WIFI_AUTO.md](WIFI_AUTO.md)，没有0.1.7十分钟长测，真实掉线重连仍未实测；当前HTTPS只读状态和一次OTA已实现并验收，不能沿用历史“HTTP未实现”的结论。
