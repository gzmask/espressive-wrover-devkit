# ESP-WROVER-KIT bring-up

ESP-WROVER-KIT v4.1 / 经典 ESP32 的 ESP-IDF 固件：FreeRTOS、PSRAM、ILI9341、自动 Wi-Fi 连接与 HTTPS OTA。

## 当前状态（2026-11-08）

**设备已通过真实 Wi-Fi 更新到 `0.1.7-ota-wifi`，运行 B 槽 `0x160000`，启动状态 VALID。阶段已验收关闭。**

- 首次 USB 安装 `0.1.6-ota-bootstrap`、固定版本的回退 bootloader、双槽分区表和 OTA 控制记录，各写一次。
- 用户确认 LCD 不再翻转后，仅一次 HTTPS POST 更新到 0.1.7，HTTP 200，无重传、无 UART 写 B。新固件软件重启，完成 30 秒连续健康确认。
- 270.037 秒过渡采集包含升级与 B 首次启动；随后另一次正常 RTS 复位、独立 180.024 秒采集及证书校验的 HTTPS 状态检查通过。
- 上传后完整 4 MiB 预期、全部授权范围外 Flash、控制记录 CRC/VALID 和 8 项安全字段通过。**保护核验在最终正常启动前**，不声称之后再次读取 Flash。
- NVS、free 和旧 JS/存储数据未变。写前 B 整槽实际为 `0xFF`，不是已有备用 Espruino 应用；原始 Espruino 整片双备份保留。
- **全部写入授权已消耗**。再次更新必须确认新镜像 hash、目标槽和准确范围；不自动重刷、恢复备份或整片擦除。

## 已实现与限制

| 项目 | 当前状态 |
| --- | --- |
| 硬件 | ESP32-D0WD rev1.0，4 MB Flash，40 MHz 晶振，CPU 160 MHz；不是 ESP32-S3 |
| PSRAM | 8 MiB 器件、4 MiB 映射；启动测试及独占 128 KiB 六模式通过，不是完整 8 MiB 测试 |
| LCD | 固定 ILI9341 / 320×240；两轮 ID4=`93 41` 后只初始化一次，未知/不一致 ID 停止 LCD，不试 ST7789 |
| Wi-Fi | 应用内嵌凭据，启动自动连接；美国 US / 信道 1–11 / MANUAL，仅 2.4 GHz；不写旧 NVS |
| HTTPS | 8443，固定服务端证书；GET `/health`、`/status` 为无需 Bearer 的只读状态，POST `/ota` 必须 Bearer 认证 |
| OTA | 只更新非运行应用槽，SHA-256/SDK 镜像检查、启动健康确认；真实升级和确认已通过，故障回退尚未注入实测 |
| 未实现 | SD、Agent、BLE、LoRa、uLisp；不提供明文 HTTP、文件管理、任意命令或网络 bootloader/分区表更新 |

原周期翻转的根因尚未确认。已移除旧 UNKNOWN-ID 双驱动轮换，用户确认 bootstrap 无翻转；B 的刷新日志不能冒充助手全程目视观察。

受控断电、真实掉线恢复、WPA3/PMF/SAE 细节、新十分钟长测及完整 8 MiB PSRAM 仍未验收。旧版 660/690 秒结果只属于各自历史版本。

最后发现的 UART 为 FTDI B `/dev/cu.usbserial-101`，A/JTAG 为 `100`；重插后必须重新枚举，115200 baud。验收结束时串口已释放，没有持续后台监控。

## 构建与测试

ESP-IDF 固定 **v5.5.6** / commit `880a203ae603d08a94d06aa01e0d6761958e5e9e`。源码默认版本仍为 **0.1.6**，可选功能默认关闭；板上 0.1.7 使用独立 OTA 配置和 `PROJECT_VER`，二者不要混淆。

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh

cmake -S tests -B .local/host-build -G "Unix Makefiles"
cmake --build .local/host-build --parallel 4
ctest --test-dir .local/host-build --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
bb tests/test_ota_upload.bb
```

当前 45 项 C ASan/UBSan、15 项 Python、4 个上传工具 mock 场景和四配置构建通过。mock 不替代上述实机证据。精确构建见 [BUILD.md](docs/BUILD.md)，OTA 配置和后续更新见 [OTA.md](docs/OTA.md)。**不要执行构建输出建议的通用 `idf.py flash` / `@flash_args`。**

凭据、token、TLS 私钥由外部私有文件链接，不放值进源码/sdkconfig/命令行。BIN/ELF/生成汇编/目标文件和当前 Flash 快照含这些秘密，只留私有、Git 忽略的 `.local/`，禁用 ccache。目录 0700，输入/日志 0600，冻结产物 0400。不要分享镜像或重新生成正在使用的 key/token。

## 文档导航

| 文档 | 用途 |
| --- | --- |
| [OTA](docs/OTA.md) | 当前双槽布局、HTTPS、已验收升级与后续更新边界 |
| [构建](docs/BUILD.md) | 固定工具链、默认关闭与 OTA 配置、测试 |
| [测试结果](docs/TEST_RESULTS.md) | 当前与历史 PASS / NOT TESTED |
| [硬件](docs/HARDWARE.md) | v4.1 接线、控制器与硬件证据 |
| [官方参考资料](docs/references/espressif/README.md) | 官方来源与本地副本校验值、引脚冲突与玩法边界；原文件不随仓库发布 |
| [烧录与恢复](docs/FLASH_AND_RECOVERY.md) | 当前恢复基线、历史授权、串口恢复 |
| [原始备份](docs/FLASH_BACKUP.md) | 迁移前双读备份与旧分区表，非当前布局 |
| [首次烧录](docs/FIRST_FLASH.md) | 0.1.0 历史 |
| [PSRAM](docs/PSRAM_PROBE.md) | 0.1.1 历史容量与有限自检 |
| [LCD](docs/LCD_PROBE.md) | 0.1.2 历史 ID/照片/诊断过程 |
| [Wi-Fi 扫描](docs/WIFI_SCAN.md) | 0.1.3 历史被动扫描 |
| [UART 配网](docs/WIFI_STA.md) | 0.1.4 历史协议，不能用于当前固件配网 |
| [自动连接](docs/WIFI_AUTO.md) | 0.1.5 历史；应用内嵌凭据沿用至今 |
| [操作与发布边界](docs/SAFETY.md) | 设备写入权限、凭据与产物保护；本机私有交接不发布 |

## 代码结构

```text
main/                        FreeRTOS 启动、心跳、健康汇总、PSRAM 自检
components/node_status/      队列接收方独占状态及 JSON
components/memory_pattern/   可在主机测试的纯内存模式算法
components/board_display/    固定 v4.1 接线、ILI9341、独立显示任务
components/wifi_service/     扫描、STA/DHCP、嵌入凭据及纯状态逻辑
components/ota_service/      HTTPS 双槽应用更新与健康确认
partitions.ota.csv           已部署的双槽分区定义
scripts/                    构建输入校验、旧配网工具、单次 OTA 上传器
tests/                      主机测试及独立构建配置
docs/                       当前说明与明确分版的历史记录
```

当前冻结与验收检查点为 `.local/flash/ota-2026-11-08-lcd-fixed/`。`current-0.1.7-expected.bin` 是重建并与实际设备完整摘要匹配的快照，**不是新双读备份**。冻结归档中的旧文档保留当时状态，不改写为新的验收结论。
