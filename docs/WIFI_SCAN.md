# Wi-Fi被动扫描诊断：历史0.1.3-wifi-scan

本页保留历史扫描镜像的独立验收；当前已通过真实Wi-Fi部署0.1.7，见 [OTA.md](OTA.md)；0.1.5历史自动连接与保护结果见 [WIFI_AUTO.md](WIFI_AUTO.md)，旧 UART 配网版见 [WIFI_STA.md](WIFI_STA.md)。以下端口/指标和“本版”仅指历史扫描版。

**已按用户对指定镜像/范围的新授权，仅烧录应用一次。** 写前整片摘要及写后镜像/全部范围外 Flash/8 项安全字段校验通过。首次启动和扫描成功，找到 8 个 AP；用户已确认新画面显示 `AP:8, SCANS:1`，明确无需照片。原 690 秒采集在最后可见日志 557.444 秒、9 轮扫描后因串口 `Device not configured` 中断，没有完成元数据，不能算 PASS。端口重枚举后确认同一设备，未再次烧录；新日志完整采集 **690.14 秒、11 轮扫描通过**，写后及运行后镜像/全部范围外 Flash/8 项安全字段均保持一致，最终正常复位并留在应用运行态。中断记录单独保留，不混入完整时长验收。

## 范围与旧分区保护

本阶段只扫描，不接收凭据、不关联 AP、不创建 IP 接口、不启动 DHCP、SoftAP、HTTP、BLE、OTA 或原始帧发送接口。默认 `CONFIG_WIFI_SERVICE_ENABLE=n`。

保留原 Espruino 启动链、分区表及 NVS。现代分区校验器拒绝旧表的多个 `data/ota` 子类型，但旧表 MD5 正确；旧 NVS 数据格式、迁移行为和共享使用权限没有验证。因此本版**不调用 `nvs_flash_init()` 或任何 NVS 擦除/写入 API**，不照搬官方示例遇到 NVS 错误就擦除的处理。

仅调用 `esp_wifi_set_storage(WIFI_STORAGE_RAM)` 仍不足以保护旧 NVS：PHY 默认会独立读取/写入校准数据。本版同时设置：

```text
CONFIG_WIFI_SERVICE_ENABLE=y
# CONFIG_ESP_WIFI_NVS_ENABLED is not set
# CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE is not set
# CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION is not set
```

此外 `wifi_init_config_t.nvs_enable=0`，驱动存储为 RAM，PHY 使用应用内置 init data，每次启动重新做完整校准。开启任一持久化选项或 PHY 数据分区时，Wi-Fi 实现的编译保护会拒绝构建；三个否定测试均已执行。

依据为锁定的 ESP-IDF v5.5.6 源码：

- `components/esp_wifi/include/esp_wifi.h`：初始化 NVS 标志、RAM 存储和扫描 API 合约。
- `components/esp_phy/Kconfig`：默认保存校准数据的行为及关闭后的完整校准。
- `components/esp_phy/src/phy_init.c::esp_phy_load_cal_and_init()`：持久化关闭时走 `PHY_RF_CAL_FULL` 分支，不调用校准 NVS load/store。

运行时保护不是仅靠配置推断：写后及原 9 轮扫描中断后，旧 NVS 所在前缀及全部范围外 Flash 已与写前摘要匹配；完整 690 秒采集后，相同范围和 8 项安全字段也已再次校验一致。被动扫描参数不等同于实验室认证的整机零射频发射；启动 PHY 完整校准可能产生射频活动。

## 扫描、并发与错误行为

- 初始化 `esp_netif` 全局支持和默认事件循环，但不创建 STA IP netif。Wi-Fi 驱动 STA 模式仅用于扫描，没有 `esp_wifi_connect()` 或凭据配置调用。
- 只读取驱动国家配置，要求文档中的世界安全默认值 `01`、起始信道 1、共 11 个信道，否则停止服务。**不猜用户所在地，不调用会持久化的 country setter。** 后续主动连接前需另行确认所在地和合规配置。
- 显式 `WIFI_SCAN_TYPE_PASSIVE`，每信道 360 ms；异步启动，经 `WIFI_EVENT_SCAN_DONE` 通知独立任务，等待上限 12 秒。每轮完成后等待 60 秒再扫描。不传 NULL 扫描配置，因为那会选择主动扫描。
- 不支持 5 GHz；本配置不扫描信道 12–14。隐藏 SSID 的被动可见性有限，不能靠本版恢复名称。
- 最多输出按 SDK 排序的 16 个 AP，同时记录总数及实际输出数；弱 AP 可能不在前 16 个中。`esp_wifi_scan_get_ap_records()` 即使截断也释放整份驱动列表；零结果和错误路径显式清理列表。
- Wi-Fi 任务优先级 1、栈 **6144 字节**，先延迟 3 秒让 LCD 首帧完成；不在 LCD、心跳或系统事件回调中执行扫描或 JSON 格式化。回调只通知所有者任务。
- 状态由 Wi-Fi 任务独占，LCD 通过内部 RAM 短临界区获取值拷贝。LCD 每帧显示 `STARTING / SCANNING / IDLE / ERROR` 和上次完成扫描的 AP 数/轮数；`IP: --` 不虚构连接。
- 任一初始化、扫描、超时、结果处理失败进入终态 `ERROR`，保留错误码并尝试停止驱动，LCD/心跳仍独立运行。**本扫描版不自动重试错误**；正常完成才定期重扫。本扫描镜像不实现 STA 退避重连；后续 0.1.4 的软件和实机验证见 [WIFI_STA.md](WIFI_STA.md)。
- 终态任务保持休眠，回调上下文保存在静态内部 RAM；即使清理失败，迟到的通知也不会指向已删除任务。不销毁共享的默认事件循环。

## UART 数据与隐私

原 `status={...}` 仍只表示心跳状态，不能将 `running` 当成 Wi-Fi 成功。新增 `wifi_status={...}` 报告扫描状态，固定 `connected=false`、`ip=null`、`rssi=null`；AP 的 RSSI 属于扫描记录，不是连接信号强度。

`scan_ap={...}` 的字段为 `ssid_hex`、`bssid_masked`、`channel`、`rssi`、`security`、`authmode_id`。SSID 以 SDK 字符串长度（首 NUL 或最多 32 字节）编码成十六进制，不直接输出不可信的终端控制字符或无效 UTF-8；隐藏名称为空。BSSID 只保留最后两字节。未知安全类型显示 `unknown`，同时保留 SDK 数值，绝不当成开放网络。

**十六进制可逆，不是匿名化。** 扫描日志可能暴露附近网络信息，只保存在权限受限且 Git 忽略的 `.local/`，不要原样公开。SDK `wifi` 标签的 INFO 级别被关闭以减少设备 MAC 输出，WARN/ERROR 保留。

在本地主机将已知 SSID 转为用于匹配的十六进制（只用于 SSID，不提供密码）：

```sh
bb -e '(println (apply str (map #(format "%02x" (bit-and % 255)) (.getBytes (first *command-line-args*) "UTF-8"))))' 'test-ssid'
```

## 历史构建与软件验证

构建命令仅适用于对应0.1.3源码/配置，不在当前源码上只改PROJECT_VER复现。当前0.1.7构建与OTA见 [BUILD.md](BUILD.md)、[OTA.md](OTA.md)。

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-wifi-scan \
  -DSDKCONFIG="$PWD/.local/sdkconfig.wifi-scan" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults;$PWD/tests/sdkconfig.lcd.defaults;$PWD/tests/sdkconfig.wifi-scan.defaults" \
  -DPROJECT_VER=0.1.3-wifi-scan -DIDF_TARGET=esp32 build
```

- **PASS** — LCD+PSRAM+扫描、LCD+扫描无 PSRAM、默认全部关闭，三个配置构建成功，无 warning/error。
- **PASS** — ASan/UBSan 主机测试 **28/28**：原 20 项、扫描状态/安全格式化 7 项、LCD 网络状态渲染 1 项。保存了先失败、后通过的测试日志。
- **PASS** — 状态转换与非法转换、计数饱和、二进制/隐藏/32 字节 SSID、基本安全类型、BSSID 遮蔽、缓冲区 0..512/保护字、逐分配点 OOM/释放、LCD 状态及计数渲染。
- **PASS** — clang-format、Wi-Fi/显示纯逻辑 Clang 分析、8 个自有 C 文件 Xtensa GCC `-fanalyzer` 无诊断；三个持久化保护均拒绝不安全配置。
- **PASS** — 实际编译响应文件保留 ESP32 revision 1 PSRAM cache workaround / MEMW；默认关闭版 ELF 不包含 Wi-Fi init/start、扫描任务或 TCP/IP init 符号。

本地证据：`.local/evidence/wifi-scan/` 下的 `build*.log`、`host-tests*.log`、`display-network-red.log`、`static-analysis.json`、`gcc-analysis-*.log`、`reject-*.log`、两个 Clang `.plist` 和 `image-info.txt`。

## 已授权并部署的镜像与写入范围

| 项目 | 值 |
| --- | --- |
| 版本 | `0.1.3-wifi-scan` / ESP-IDF v5.5.6 / ESP32 |
| 构建产物 | `.local/build-wifi-scan/wrover_radio_node.bin` |
| 冻结副本 | `.local/flash/wifi-scan-2026-11-07/candidate-app.bin` |
| 大小 | **855312 / 0xD0D10 字节** |
| SHA-256 | `e458183c39616b2cba23cb8f01110c18e71ef5189d26b7cbd1cbc15808988b6a` |
| 应用起点 / 镜像终点（不含） | `0x10000 / 0xE0D10` |
| 实际擦写扇区范围 | **`[0x10000, 0xE1000)`，209 个扇区** |
| 镜像头 | DIO、40 MHz、4 MB、最低 revision 1.0，checksum/hash 有效 |

此前仅到 `0x53000` 的 LCD 授权没有沿用。明确请求本冻结镜像和新范围后，用户回复 **“yes”**，才执行应用专用写入；855312 字节用时 46.9 秒，数据 hash 校验通过。新范围位于原 factory 槽内，会覆盖更多旧应用残留，原应用备份保留。仍不写 bootloader、分区表、NVS 或其他范围外数据，不擦整片、不改 eFuse、不恢复备份。禁止 `idf.py flash` / `@flash_args`。

准备阶段已从旧快照和冻结的 LCD 镜像重建预期当前全片：`.local/flash/wifi-scan-2026-11-07/preflash-0.1.2-expected.bin`，SHA-256 `ef23afa09e268f1c4a7900f924f43f671aacb09b088487c42a4ce46c1b88c85c`。这是重建文件，已在实际写入前用设备整片摘要验证通过，**不是又完成一次整片双读**。冻结的候选 ELF、源代码压缩包和有效 sdkconfig 与镜像同目录保留。设备身份及 8 项安全字段与历史记录一致；写后和中断后均已校验新镜像、全部授权范围外 Flash 和安全字段。完整长时采集结束后已再次独立核验三段摘要及安全字段，均匹配；随后正常复位并完成最终 45 秒启动检查，不是沿用旧 LCD 版的 PASS。

## 实机采集、内存与保护证据

| 采集 | 时长 | 完成扫描轮数 | 心跳 | 结论 |
| --- | --- | --- | --- | --- |
| `boot-1` | 45.17 秒 | 1 | 1→42 | PASS |
| `boot-2-soak` | 最后日志 557.444 秒，无完成元数据 | 9 | 1→557 | INCOMPLETE：UART 设备消失，不算 690 秒通过 |
| `boot-2-soak-retry` | **690.135 秒** | **11** | **1→687** | PASS：独立、连续完整采集 |
| `boot-3` | 45.17 秒 | 1 | 1→42 | PASS：运行后复核后正常启动 |

完整长时采集的 138 个 running 样本跨度 685.994 秒；69 个 ILI9341 帧日志计数 1→681，跨度 679.990 秒。11 轮各 4–7 个 AP，共 58 条重复扫描观测，**不表示 58 个独立 AP**；结果信道 1/3/6/9，RSSI -92..-46 dBm，安全类型包括 WPA2-PSK、WPA2-Enterprise 和 WPA2/WPA3 混合。这里只报告 beacon 扫描属性，没有进行认证/加密协商。

关联同一日志时间轴的扫描状态，排除 idle 发布后首 1 秒的结果处理瞬态，126 个轮间样本的总空闲 heap 都为 **4391744 字节**、内部 **199836 字节**，无持续下降。最低总 heap **4258552 字节**；LCD / Wi-Fi / 状态任务栈低水位分别 **4088 / 3624 / 1576 字节**。PSRAM 自检前后外部空闲均为 **4192024 字节**，仍只测试独占分配的 128 KiB，未全面测试 8 MiB。

原采集失败为主机串口读取和清理 RTS 时的 `Device not configured`；根因未定位，不归咎于固件、线材或用户。17:38 UTC 重新发现 UART `/dev/cu.usbserial-20301` 后核验同一设备和保护数据；完整重测期间使用进程作用域的 `caffeinate -i` 防空闲休眠，进程结束自动解除，不修改系统设置，也不承诺阻止手动合盖/断开 USB。

原始证据保存在 `.local/flash/wifi-scan-2026-11-07/`：`boot-*.log/.json`、`boots-summary.json`、`soak-summary.json`、`interrupted-summary.json`、`soak-interruption.json`、`operator-observation.txt`、`postflash-range-verify.log`、`post-interrupted-range-verify.log`、`post-soak-range-verify.log` 和三阶段安全 JSON；重连身份记录在子目录 `reconnect-20261107-1738/`。`scripts/summarize-capture.bb` 从原始日志输出不含 SSID/BSSID 的汇总；经交互求值验证、clj-kondo/cljfmt 检查，完整及不完整采集分别得到正确 PASS/INCOMPLETE 结果。目录 `SHA256SUMS` 记录最终证据摘要。

## 实机验收与后续阶段

- [x] **PASS（完整启动）** — 三次完整采集均经原 bootloader 启动，PSRAM 启动/128 KiB 自检、两轮 ILI9341 ID 和 Wi-Fi 初始化/扫描通过；另一次中断采集单列。
- [x] **PASS（扫描格式/范围）** — 多轮 AP 属性检查通过，观测结果信道处于配置的 1–11 内，RSSI/安全类型合理；不宣称某个已知 SSID 已被操作者匹配。
- [x] **PASS（后续本地比对）** — 用户提供目标配置后，输入第一行 SSID 与本地历史 WPA2-PSK 扫描记录匹配；不回显名称，不代表当前 AP 仍可用或密码正确。
- [x] **PASS（长时并发）** — 完整 690.14 秒、多轮扫描、稳定轮间 heap、LCD/心跳进度、栈低水位及无 panic/watchdog/stalled/异常复位。
- [x] **PASS（操作者确认）** — 用户表示“可以看到，不需要照片。ap:8, scans: 1”；这是新版本的可见画面确认，不再要求照片，也不声称存在新照片或连续视频。
- [x] **PASS（保护/收尾）** — 写前整片、写后/中断后/完整长时运行后的新镜像、全部范围外 Flash 及安全字段匹配，包含旧 NVS 未变；最终正常启动后释放 UART、保留应用运行。
- [ ] **NOT TESTED（实机）** — 授权 STA 连接、WPA2/WPA3 协商、DHCP、退避重连；本扫描镜像不实现这些功能。后续 0.1.4 已另行获准部署并验证指定网络关联/DHCP、在线并发及数据保护，见 [WIFI_STA.md](WIFI_STA.md)；真实掉线重连仍未实测；“无HTTP”仅指本扫描版，当前0.1.7的HTTPS状态及OTA已实测，见 [OTA.md](OTA.md)。

本次扫描验收时没有网络凭据，也没有关联尝试。后续用户已提供本地凭据文件并确认美国所在地，新 station 候选采用一次性本地 USB 输入；凭据只留在权限 0600 的本地文件及运行时 RAM，不编译进固件、不回显、不进入公开文档，也不使用已存的 Espruino 凭据。是否启用新 NVS 存储需另行规划/授权，绝不静默擦除或迁移旧 NVS。
