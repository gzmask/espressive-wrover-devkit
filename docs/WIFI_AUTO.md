# 上电自动连接：历史0.1.5-wifi-auto

本页保留0.1.5独立历史实测。当前已通过真实Wi-Fi部署并验收0.1.7/B/VALID，见 [OTA.md](OTA.md)。复现准确 0.1.5 镜像需使用其冻结源码，不是仅在新源码上设置旧 PROJECT_VER。

用户明确要求“wifi密码写进固件就行，别过度engineering”。因此不增加 NVS 配网、加密存储或管理接口，直接将现有本地两行文件（SSID、密码）嵌入应用；启动后自动加载并连接。**本历史阶段当时部署0.1.5-wifi-auto；用户对准确 hash/范围回复 yes 后，仅写应用一次，并验证两次无 UART 凭据输入的自动连接。**

## 改动

- 用 ESP-IDF `target_add_binary_data` 从外部本地文件链接凭据，不在 Git 源码/sdkconfig 中写值，不生成含密码的 C 头文件。
- 删除 station 启动时的 UART nonce/CRC 收包和 120 秒配网等待；保留 LCD/心跳先启动的 3 秒延迟。
- 每次复位都从应用镜像读取凭据，再按现有 STA/DHCP 流程连接。成功后不需要电脑持续连接；供电仍须保持。
- 保留 US / 1–11 / MANUAL、WPA2 最低门槛、有界退避，以及现有 LCD/PSRAM 功能。
- 仍使用 `WIFI_STORAGE_RAM`、禁用 Wi-Fi/PHY/DHCP NVS 持久化；它指驱动不写 NVS，**不再表示凭据不会留在 Flash**。该0.1.5阶段没有修改旧bootloader/表/NVS；后续OTA迁移已单独批准并完成，当前仍不写NVS。
- 原 `wifi_provision` 的格式校验被复用。旧 UART 帧代码及 `scripts/provision_wifi.py` 保留供历史 0.1.4 使用；该脚本的 `--validate-only` 也供构建时检查本地文件。本候选运行时不接收 UART 凭据。

**应用 bin、ELF、生成的汇编/目标文件都含可提取的明文凭据，只放权限受限的 `.local/`，不要提交或分享。** 用户已接受凭据在固件中，不做密码加密；不回显 SSID/密码，也不发布原凭据文件摘要。以后改路由器名称/密码，需要重新构建和烧录。

## 历史构建

以下命令使用0.1.5对应历史源码/配置，不代表当前OTA构建；完整现行参数见 [BUILD.md](BUILD.md) 和 [OTA.md](OTA.md)。

输入必须是当前用户持有的 0600 普通文件，两行分别为 SSID 和密码。支持 LF/CRLF 和可选末尾换行，保留首尾空格。构建时传文件路径，不传凭据值：

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-wifi-auto \
  -DSDKCONFIG="$PWD/.local/sdkconfig.wifi-auto" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults;$PWD/tests/sdkconfig.lcd.defaults;$PWD/tests/sdkconfig.wifi-sta.defaults" \
  -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt -DCCACHE_ENABLE=0 \
  -DPROJECT_VER=0.1.5-wifi-auto -DIDF_TARGET=esp32 build
```

不提供文件或文件格式/权限不合格，STA 配置失败；默认关闭和扫描构建不需要凭据。不要用当前源码只更改 PROJECT_VER 来复现旧 UART 配网版，需使用其冻结源码。

## 软件验证与部署边界

- **44/44** C 主机测试（ASan/UBSan）：原 41 项，加两行解析的有效输入、无效输入及长度/PSK 边界 3 项；先失败再实现。
- **11/11** 原 Python 主机测试通过，保持旧工具可用。
- STA+LCD+PSRAM、默认全关闭、扫描回归三个配置构建通过；格式和凭据解析 Clang 静态分析通过。
- 实际私有文件在构建时通过格式/权限验证，链接器有嵌入数据及长度符号。软件冻结后才按单独批准进行设备操作。
- 已部署镜像 879376 字节，从 `0x10000` 到 `0xE6B10`（不含），实际仅擦写 **`[0x10000, 0xE7000)`**，215 扇区；一次写入 48.3 秒、数据 hash 校验通过。冻结文件及私有元数据在 `.local/flash/wifi-auto-2026-11-07/`。
- 本次已对新 hash/范围独立获准，没有复用 0.1.4 授权。该次写入许可已消耗，不能重刷或改范围；未写 bootloader、分区表、旧 NVS 或 eFuse。

## 自动连接实测

| 正常 RTS 复位 | 完整采集 | 首次 ONLINE | UART 凭据输入 | 关联/DHCP |
| --- | --- | --- | --- | --- |
| 第一次 | 60.0984 秒 | 启动日志约 8.233 秒 | 0 | 第一次尝试成功 |
| 第二次 | 60.1013 秒 | 启动日志约 8.223 秒 | 0 | 第一次尝试成功 |

两次各 28 项短采集判据通过：应用版本正确，每次仅一次嵌入凭据加载，无旧协议 challenge/ACK；US / 1–11 / MANUAL 读回，实测信道 9。各有 11 个 online 样本，跨度 46.850 / 46.860 秒，未发生错误、重连、panic、watchdog、stalled 或异常复位。

两次心跳均 1→57（跨度 55.993 秒）、ILI9341 提交均 1→51（50 秒）；两轮 ID4=93 41、PSRAM 启动及 128 KiB 六模式测试通过，外部空闲前后均 4192024。LCD/STA/状态栈低水位 3984/5440/1696 字节。LCD 依据完成提交日志，不声称新照片或全程人工目视观察。

写前核对同一私有 MAC、ESP32 revision 1.0、40 MHz、4 MB、8 项安全字段，并与实际设备完整 4 MiB 摘要匹配。写后及首轮 60 秒运行后，应用、全部范围外 Flash、8 项安全字段保持一致；额外重建 `current-0.1.5-expected.bin` 并与实际设备完整 4 MiB 摘要匹配，含擦除填充区。不是新双读，且该快照也含明文凭据，只限私有保存。

保护核验在第二次正常启动之前完成，之后最终 60 秒自动连接通过、留在 ONLINE、UART 释放，无后台采集/监控。没有声称最终启动后又检查过 Flash。

当时0.1.5以及当前0.1.7运行都不需要电脑或USB配网，只需要可靠供电；重启会使用应用内的同一网络凭据。**尚未做受控断电恢复、本版十分钟长测、真实掉线退避恢复或 WPA3/PMF/SAE 协商细节验证。** 旧 0.1.4 的 690 秒结果不算作本版长测，短采集的长时 heap 判据不适用。

本历史0.1.5未实现SD/HTTP/OTA；后续0.1.7已实际通过HTTPS更新验收，见 [OTA.md](OTA.md)。不能把0.1.5联网结果当作OTA证据。
