# Wi-Fi STA / DHCP：历史0.1.4-wifi-sta

**本页记录旧 0.1.4 的历史构建/运行方式。** 用户后续要求将凭据编入固件；0.1.5曾独立验证两次自动连接，当前已通过真实Wi-Fi部署0.1.7，见 [OTA.md](OTA.md)；历史自动连接结果见 [WIFI_AUTO.md](WIFI_AUTO.md)。本页的旧构建命令需配合冻结的 0.1.4 源码，不能只在新源码上设置旧 PROJECT_VER 来复现。

用户提供本地凭据文件、明确确认设备在美国，并对本版准确 hash/大小/擦写范围及 USB 凭据/DHCP 请求回复“允许，继续”后，仅写入应用一次。**本阶段板上是 0.1.4-wifi-sta：三次正常启动均第一次尝试关联指定 AP 并取得 DHCP 地址，独立 690.15 秒在线并发及数据保护通过。** 另做一次无凭据复位超时测试；最终重新提供凭据、45.11 秒连接采集通过，保持 ONLINE，UART 已释放。

本地文件格式为第一行 SSID / 第二行密码，权限 0600；目标 SSID 先前已与历史 WPA2-PSK 扫描记录私下匹配。没有回显网络名称或密码，没有复制凭据进项目；**复位后需要通过可信本地 USB 重新提供凭据**，不自动连接旧网络。真实掉线重连、WPA3/PMF/SAE 协商细节及受控断电恢复仍未实测；该0.1.4未实现HTTP；当前HTTPS状态/OTA已独立验收，不沿用这一历史限制。

## 仅0.1.4的RAM/UART凭据模式（当前已不用）

`WIFI_STORAGE_RAM` 只决定 Wi-Fi 配置的驱动存储方式；把密码写进 C 源码、sdkconfig 或生成头文件仍会使它留在应用 Flash。因此本版**构建不读取凭据文件，不内置任何 SSID/密码**：

1. 显式启用 station 模式后，独立任务进入 `config`，先给 LCD/心跳 3 秒启动时间。
2. 复用已确认的 UART0 / 115200 / GPIO1、3，不改引脚或串口波特率；只安装临时 RX 驱动，不调用回显/REPL，不增加任意命令执行接口。
3. 输出 `provision_ready v=1 nonce=... firmware=0.1.4-wifi-sta country=US`。配置总窗口 120 秒，未收到合法帧则终止，不初始化网络。
4. 上位机只在匹配固件、协议、US 和启动令牌后发送一次有界二进制帧。SSID 最多 32 字节；密码为 8–63 个可打印 ASCII 字节，或 WPA2 的 64 位十六进制 PSK。保留首尾空格，不做 trim；禁止控制字符。
5. 固件校验 magic、长度、启动令牌、CRC32 和内容，设置 RAM-only 驱动配置，擦零应用帧/凭据/临时配置缓冲，再删除临时 UART RX 驱动。SDK 为重连继续持有内存中的配置，复位后丢失；不会再次打开配网窗口或自动读取其他凭据。

协议为 `WRV1` + 8 字节小端启动令牌 + SSID/密码长度各 1 字节 + 两段原始字节 + 小端 IEEE CRC32，总长最多 114 字节。**CRC 和令牌不是身份认证或加密。USB 必须是可信本地物理连接，不能将此协议开放到网络；不声称 Python、SDK 或释放后的 RAM 副本均得到安全擦除。** 它不是 Agent 集成、远程管理协议或安全启动/设备认证方案。

本地工具 `scripts/provision_wifi.py` 使用 pyserial，因此采用 Python，其他数据整理仍可用 Babashka。工具拒绝非当前用户持有/有组或其他用户权限/符号链接的凭据文件，日志目录必须已存在且仅所有者可访问；日志及元数据用独占创建、0600，不覆盖旧证据。

工具不记录发送帧；收到原文或十六进制密码时整行遮蔽并停止。意外复位不会重发凭据；串口失败后仍尽力关闭端口、保存不含秘密的错误类型/元数据，清理失败不掩盖原错误。该过滤只覆盖已说明的表示形式，不是任意第三方输出的通用秘密检测器。

只做本地格式检查（不打开串口）：

```sh
.local/espressif/python_env/idf5.5_py3.12_env/bin/python scripts/provision_wifi.py \
  --credentials /path/to/owner-only-wifi.txt --validate-only
```

下面仅为历史0.1.4配网/采集示例，会正常复位并中断应用；**不要对当前0.1.7、扫描版或身份未确认的设备运行**。当前固件没有该challenge接口，自动加载应用内凭据。新镜像/再次写入仍需独立授权及设备身份、写入保护核对，不能把以下串口操作当成烧录许可：

```sh
# 端口必须重新发现；目录须为 0700，输入文件须为 0600。
caffeinate -i python scripts/provision_wifi.py \
  --credentials /path/to/owner-only-wifi.txt \
  --port /dev/cu.usbserial-20301 \
  --log .local/flash/wifi-sta-2026-11-07/boot-1.log --seconds 690
```

## 保留旧分区与美国法规设置

保留原 Espruino bootloader、分区表、NVS 和其他数据。旧表的重复 `data/ota` 子类型没有迁移。本版不初始化、擦除或迁移 NVS，不使用旧存的 Espruino 凭据。

除 `nvs_enable=0` 和 `WIFI_STORAGE_RAM` 外，编译保护拒绝以下任一配置：

- `ESP_WIFI_NVS_ENABLED`
- `ESP_PHY_CALIBRATION_AND_DATA_STORAGE`
- `ESP_PHY_INIT_DATA_IN_PARTITION`
- `ESP_PHY_MULTIPLE_INIT_DATA_BIN`
- `LWIP_DHCP_RESTORE_LAST_IP`：DHCP 也存在独立的旧租约保存路径，不能只禁用 Wi-Fi/PHY NVS。

`WIFI_SERVICE_US_CONFIRMED=y` 是对实际部署位置的明确确认；不确认则编译失败。设置 `esp_wifi_set_country_code("US", false)`，读回必须为 US / 信道 1–11 / MANUAL，拒绝 AP 的 802.11d 改写；不能从电脑时区推断国家，也不是射频合规认证。

ESP-IDF API 文档说明 country/config setter 可能保存 Flash，不能仅凭 API 名称推断无写入。锁定版本的 `components/esp_wifi/test_apps/wifi_function/main/test_wifi_country.c` 明确以 `nvs_enable=false` 测试 country setter；本版先选择 RAM storage，禁用上述持久化配置，再调用 setter。本次已实测：写后及 690 秒在线/无凭据超时测试后，全部范围外 Flash（含旧 NVS）、8 项安全字段均保持不变，另完成当前完整 4 MiB 预期快照摘要校验；保护复核在最终连接启动前完成。PHY 每次重新启动会完整校准；连接扫描会主动发送，不能沿用被动扫描的射频描述。

## 状态、DHCP 与重连

- 扫描模式和 station 模式互斥，默认都关闭。Station 独占任务优先级 1、栈 8192 字节；LCD、心跳/状态任务不变。
- `config → connecting → dhcp → online`；关联不等于连接成功。只有当前 SDK STA 关联、目标 SSID/允许的安全类型/信道检查通过，netif 已 up、当前非零 IPv4 与事件一致后才报告 online。
- 配置 `WIFI_AUTH_WPA2_PSK` 最低门槛、PMF capable、不要求旧 WPA2 AP 支持 PMF；编译 SAE 路径，尚未验证 WPA3 协商。拒绝开放、WEP、WPA-only 或 Enterprise 作为本凭据模式的目标，不自动降级。
- 每次关联最多 25 秒，DHCP 最多 30 秒。断线/丢 IP 立即清除所发布的关联、IP/RSSI；在线时每 5 秒同时检查实际关联和当前租约，避免仅等延迟 IP_LOST 事件而长期报告旧 IP。
- 失败后先停止驱动，收到 STA_STOP 后才允许下一次启动；停止确认最多 5 秒。默认 netif 处理器停止 DHCP/清理旧 IP；下一次启动前必须读到零地址，不复用旧租约。
- 重试延迟 1、2、4、8、16、30 秒，第 7 次连续失败进入终态 ERROR，直到复位；SDK 内部额外重试设为 0。只有 online 持续 60 秒才清零失败连击，防止反复短连导致重试风暴。不是无限重连服务。
- SDK 回调只把数字事件复制到长度 16 的静态内部 RAM 队列，不拷贝 SSID/密码/BSSID。队列满显式终止，不能静默丢事件；迟到 IP 事件必须与当前 SDK 状态相符，错误路径不释放仍被回调引用的队列/netif/所有者任务。
- 终态尝试停止无线，保留 LCD/心跳；不自动擦 NVS、恢复备份、切换国家/网络或降低安全门槛。清理状态不明则报告需复位，不自动重试。

`wifi_status` 新增 `mode=station`、country、关联/有 IP 状态、IPv4、RSSI、信道、尝试/失败数、退避时间及数值错误/断线原因，不含 SSID、密码或 BSSID。节点 `status` 仍独立，不能把 heartbeat running 当作 Wi-Fi 成功。LCD 显示 CONFIG/CONNECTING/DHCP/ONLINE/BACKOFF/ERROR，在线显示 IP/RSSI，非在线显示 `IP:--` 与尝试数；不回显凭据。

## 构建与软件验证

```sh
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-wifi-sta \
  -DSDKCONFIG="$PWD/.local/sdkconfig.wifi-sta" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults;$PWD/tests/sdkconfig.lcd.defaults;$PWD/tests/sdkconfig.wifi-sta.defaults" \
  -DPROJECT_VER=0.1.4-wifi-sta -DIDF_TARGET=esp32 build

cmake --build .local/host-build --parallel 4
ctest --test-dir .local/host-build --output-on-failure
python -m unittest discover -s tests -p test_wifi_provision_host.py
```

- **PASS** — 41/41 C 主机测试，ASan/UBSan：原 28 项 + 连接模型 9、凭据协议 3、LCD station 1。
- **PASS** — 11/11 Python 工具测试，全部用合成凭据/模拟串口；覆盖帧、边界、文件权限、challenge、遮蔽、完整采集、错误版本/无 challenge 不发送、串口/清理失败仍保存元数据、复位不重发及密码回显阻断。
- **PASS** — Station+LCD+PSRAM、station+LCD 无 PSRAM、保留的扫描模式、默认全部关闭，4 配置构建，无 warning/error。
- **PASS** — clang-format；新模型/协议/显示纯逻辑 Clang 分析；11 个自有 C 文件 GCC -fanalyzer，加扫描分支共 12 次编译，无诊断。
- **PASS** — 5 个持久化选项、US 未确认及非默认 UART0，共 7 个否定编译检查。
- **PASS** — 默认关闭 ELF 不含 Wi-Fi init/start/connect、TCP/IP init 或 station 所有者入口。保留 ESP32 revision 1 PSRAM cache workaround/MEMW 配置。

证据在 `.local/evidence/wifi-sta/`；没有用模拟结果代替实际关联、DHCP 或串口传输成功。

## 已授权仅写一次的冻结镜像

| 项目 | 值 |
| --- | --- |
| 版本 | `0.1.4-wifi-sta` / ESP32 / IDF v5.5.6 |
| 冻结目录 | `.local/flash/wifi-sta-2026-11-07/` |
| 镜像 / 大小 | `candidate-app.bin` / **893280 / 0xDA160 字节** |
| SHA-256 | `09a63fa3a8b113a31ffa2317edc5803efb3512ac94dee19ad0fde7b4f88d8353` |
| 起点 / 镜像终点（不含） | `0x10000 / 0xEA160` |
| 实际擦写扇区范围 | **`[0x10000, 0xEB000)`，219 个扇区** |
| 镜像头 | DIO / 40 MHz / 4 MB / 最低 revision 1.0；checksum/hash 有效 |

**本次准确 hash/大小/范围已独立获准，并且只写入一次**：49.0 秒、`Hash of data verified.`。没有沿用 0.1.3 的批准，也不授权再次写入。范围仍在原 factory 内，多覆盖的 10 个扇区旧应用残留已在原快照中保留。未执行 `idf.py flash`、`@flash_args`、整片擦除、bootloader/分区表/eFuse 修改或备份恢复。

写前重建 `preflash-0.1.3-expected.bin`，SHA-256 `48ec8f93445c8c83f68f593c70c8cf938b43f8adec25caf248124a33e770d8ef`；确认同一私有 MAC、ESP32-D0WD revision 1.0、40 MHz、4 MB 及 8 项安全字段后，**实际设备完整 4 MiB 摘要匹配**才写入。这是重建后核验，不是再次整片双读。

写后及 690 秒在线/135 秒无凭据测试后，分别核验应用、前缀 `[0,0x10000)`、后缀 `[0xEB000,0x400000)` 和 8 项安全字段，均匹配。额外重建 `current-0.1.4-expected.bin` 并与实际设备完整 4 MiB 摘要核对一致，包含擦除填充区。之后才进行最终 45.11 秒正常启动/重新提供凭据，保持在线；不声称最终启动后又检查过 Flash。

审计中一个冗余文本断言误用了其他工具版本的成功措辞；原始写前日志已经是 esptool exit 0、`verify OK (digest matched)`，不是设备差异。仅修正了解析判断，没有重复烧录；私有 `preflight-audit-note.txt` 保留说明。

## 实机采集与验收

| 采集 | 时长 | 结果 |
| --- | --- | --- |
| `boot-1` | 45.0029 秒 | PSRAM/ILI9341、USB 帧、US 读回、首次尝试关联/DHCP |
| `boot-2-soak` | 690.1504 秒 | 独立在线并发，137 个 online 样本、跨度 678.370 秒；无异常复位/错误/重连 |
| `boot-no-credentials` | 135.1956 秒 | 正常复位、无 UART 命令；120 秒配置超时，零连接尝试/无 IP，LCD 与心跳继续 |
| `boot-3` | 45.1142 秒 | 保护复核后再次正常复位、重新传入凭据；首次尝试关联/DHCP，通过后保持 ONLINE |

三次提供凭据启动和额外一次无凭据启动的 PSRAM 启动测试/128 KiB 六模式测试均通过，外部空闲前后均 4192024 字节；每次两轮 indexed ID4=93 41、一次 ILI9341 选择。4 个启动令牌均存在且不同；这不将令牌提升为密码学身份认证。

长时采集：137 个 running 样本，心跳 **1→685 / 683.993 秒**；69 个 ILI9341 提交日志，计数 **1→681 / 680 秒**。没有 panic、watchdog、brownout、stalled、队列溢出或异常复位。在线信道 9，RSSI -49..-43 dBm。没有输出真实 IP、SSID、密码或 MAC。

启动在线一分钟后取 124 个节点样本，总空闲 heap **4388284–4388304**、内部 **196376–196396** 字节；第 2–11 分钟的中位数固定为 **4388304 / 196396**，没有持续下降。最低总 heap **4248516** 字节（包含启动自检等阶段）；LCD / STA / 状态任务栈低水位 **3984 / 5440 / 1664** 字节。LCD 结论依据驱动完成提交及旧版已确认的接线/ID，不冒充新照片或全程人工目视观察。

无凭据测试：CONFIG→ERROR、`ESP_ERR_TIMEOUT`，连接尝试数及 IP 均为零，没有 PHY/网络初始化日志；135 秒期间心跳 1→132、LCD 提交 1→131。它验证了本次复位不自动连接旧网络/沿用之前提供的配置；不等价于对所有 RAM 副本进行取证式安全擦除证明。

所有采集均独立完整，不拼接；主机使用进程作用域 caffeinate -i，结束自动解除，无系统设置修改。三次提供凭据的采集分别有一次 challenge/ACK，UART 原始日志仅留权限受限的 `.local/`。新的私有 `.local/flash/wifi-sta-2026-11-07/scripts/summarize-station.bb` 先求值验证合成样例再保存，clj-kondo/cljfmt 通过，生成 27 项检查和脱敏汇总。不可将旧扫描汇总器的 connected=false / 无 UART 命令判据直接用于本版。

### 已通过与剩余项

- [x] 原 bootloader 启动、USB 凭据帧接收、US 手动策略读回、指定 AP 的实际关联/DHCP。
- [x] 三次正常复位/重新提供凭据，额外无输入启动不沿用旧配置并按时终止配网窗口。
- [x] 至少 10 分钟在线、heap/栈、LCD 提交/心跳并发；写后/运行后应用、范围外 Flash 和安全字段不变。
- [ ] 真实断线/丢 IP、退避、恢复和耗尽重试；没有为此擅自重启路由器、更改主机网络或干扰其他用户。模型测试不能替代真实故障验收。
- [ ] WPA3、PMF/SAE 协商细节、受控 USB 断电恢复、完整端口监听扫描未验证。源码没有启动 HTTP 服务，不把这一点冒充全端口实测。
- [ ] 本历史0.1.4未实现HTTP；当前0.1.7已实现HTTPS状态和上传，GET/status实测通过；GET/health同handler但未单独硬件调用，见 [OTA.md](OTA.md)。

采集工具的 completed 只表示预期采集/凭据确认流程完成，不等于认证/DHCP验收；必须检查实际 online/IP 事件和独立保护证据。不再要求补旧 LCD 照片；如新版本画面异常，先记录现象，不带电拆屏或随机调整引脚。
