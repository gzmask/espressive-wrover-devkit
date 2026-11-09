# 构建、测试与工具链

## 当前部署与构建配置

截至 2026-11-08，设备实际运行 **`0.1.7-ota-wifi` / OTA B / VALID**，已通过一次真实 HTTPS 更新、健康确认和后续正常复位，见 [OTA.md](OTA.md)。全部写入许可已消耗，构建不是再次烧录授权。

根 `CMakeLists.txt` 的默认 `PROJECT_VER` 仍是 **0.1.6**；可选 PSRAM/LCD/Wi-Fi/OTA 默认关闭。默认构建不等于上板配置，也不能作为可持续 OTA 的替代镜像。

| 配置 | 构建目录 | 特征 |
| --- | --- | --- |
| 已部署 bootstrap | `.local/build-ota-bootstrap` | 0.1.6、PSRAM/LCD/STA/HTTPS、回退 bootloader、双槽表 |
| 已部署 Wi-Fi 更新 | `.local/build-ota-wifi` | 0.1.7、同一 OTA 配置和私有输入，实际运行于 B |
| 默认关闭回归 | `.local/build-ota-off` | 不链接凭据/OTA 私密输入，不启动 Wi-Fi/HTTPS |
| 扫描回归 | `.local/build-ota-scan` | 扫描，无嵌入 Wi-Fi/OTA 凭据及 HTTPS 入口 |

这些目录包含已验证产物；不要为普通试验覆盖它们。冻结副本在 `.local/flash/ota-2026-11-08-lcd-fixed/`，不可修改。复现历史镜像须使用相应冻结源码/配置/私有输入，不是只把当前源码的版本标签改回旧值。

## 锁定版本

| 工具 | 本机实测版本 |
| --- | --- |
| ESP-IDF | **v5.5.6** |
| ESP-IDF commit | `880a203ae603d08a94d06aa01e0d6761958e5e9e` |
| Xtensa GCC | `esp-14.2.0_20260121` / GCC 14.2.0 |
| CMake | 3.28.2 |
| Ninja | pip 1.13.0；命令输出 `1.13.0.git.kitware.jobserver-pipe-1` |
| Python | 3.12.10 |
| IDF esptool / espefuse | 4.13.0；旧独立探测用的 5.1.0 仅属历史 |
| 主机 C 编译器 | AppleClang 21.0.0.21000401 |
| clang-format | 18.1.8 |
| IDF cJSON submodule | `b2890c8d76bbb64e710585ebc0a917196b9c67e7` |

使用固定 release，不跟随 `stable` 别名。根 CMake 拒绝非 `esp32` 目标及不匹配的 IDF commit。上游源码不自行打补丁。参考 [官方 release](https://github.com/espressif/esp-idf/releases/tag/v5.5.6)。

## 首次安装（macOS / Linux）

本机实测 macOS arm64；Linux 步骤未在本项目实测。先安装 Git、Python、CMake、主机编译器；macOS 需要 Xcode Command Line Tools，其他依赖见 [官方安装说明](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/get-started/linux-macos-setup-legacy.html)。已有目录不要重复克隆或重装。

```sh
umask 077
mkdir -p .local
git clone --branch v5.5.6 --depth 1 --recursive --shallow-submodules \
  https://github.com/espressif/esp-idf.git .local/esp-idf
git -C .local/esp-idf rev-parse HEAD
export IDF_TOOLS_PATH="$PWD/.local/espressif"
.local/esp-idf/install.sh esp32
. .local/esp-idf/export.sh
python -m pip install ninja==1.13.0 clang-format==18.1.8
```

工具和 Python 虚拟环境位于 `.local/`，不修改 shell 启动文件或全局 Python。本机安装脚本未提供可用 Ninja，故在 IDF 虚拟环境补装。Python 清单见 `.local/evidence/idf-python-freeze.txt`，上游约束见 `.local/espressif/espidf.constraints.v5.5.txt`。

## 默认关闭构建（仅编译）

以下使用新的独立目录，不覆盖已部署构建；运行前确认该试验目录未被用于其他配置。

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py --version
idf.py -B .local/build-default-example \
  -DSDKCONFIG="$PWD/.local/sdkconfig.default-example" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults" \
  -DCCACHE_ENABLE=0 -DIDF_TARGET=esp32 build
idf.py -B .local/build-default-example size
```

产物为该目录的 `wrover_radio_node.bin`。默认配置生成标准 single-app 表，应用容量 1 MiB；**它不是设备当前表**。已部署 OTA 配置使用 `partitions.ota.csv`，A/B 各 `0x150000` / 1344 KiB，详见 [OTA.md](OTA.md)。不把默认生成的表覆盖实物表。

**不执行构建输出建议的 `idf.py flash` / `@flash_args`，不整片擦除。** 本次特定引导/表迁移已获准且已执行，不能作为未来通用 flash 许可。写入和恢复见 [FLASH_AND_RECOVERY.md](FLASH_AND_RECOVERY.md)。

## 当前 OTA 配置与私有输入

OTA 配置按顺序组合：

1. `sdkconfig.defaults`
2. `tests/sdkconfig.psram.defaults`
3. `tests/sdkconfig.lcd.defaults`
4. `tests/sdkconfig.wifi-sta.defaults`
5. `tests/sdkconfig.ota.defaults`

完整 CMake 参数见 [OTA.md 的构建说明](OTA.md#私有材料与构建)。外部输入为两行 Wi-Fi 文件、32 字节 token、服务端证书及 TLS 私钥；所有者为当前用户，普通文件 0600，私有目录 0700。只传文件路径，不把值写入源码/sdkconfig/命令行。

- BIN/ELF/生成 ASM/OBJ 含上述秘密，只留 `.local/`，禁用 ccache；冻结文件 0400，不分享。
- A/B 镜像共用证书、key、token，不能任意重新生成。源码归档不含外部秘密，单独使用不能重建相同字节。
- 保留经典 ESP32 rev1/MEMW、CPU160 MHz、Flash DIO/40 MHz/4 MB、PSRAM40 MHz；OTA bootloader 禁用 1.8→1.9V LDO boost，不尝试改电压。
- 当前 B 已 VALID；未来新镜像的目标应为非运行 A，但仍需新 hash/范围批准。只上传包含对应 OTA 健康确认逻辑的配置；项目名/IDF 匹配不代表任意默认镜像能完成确认。
- 当前布局现代校验通过；多个 `data/ota` 的错误属于迁移前 Espruino 表历史，不是当前布局错误。

## 主机测试与静态检查

已通过 45 项 C ASan/UBSan、15 项 Python、4 个上传器 mock 场景及上述四配置构建。C 测试涵盖状态/JSON、内存模式、LCD 识别/固定驱动计划/渲染、STA/退避、旧 UART 帧、嵌入凭据及 OTA 纯策略；Python 含官方分区生成器验证和 pyserial 模拟；上传器使用假 curl，不操作设备。

```sh
cmake -S tests -B .local/host-build -G "Unix Makefiles"
cmake --build .local/host-build --parallel 4
ctest --test-dir .local/host-build --output-on-failure
python -m unittest discover -s tests -p 'test_*.py'
bb tests/test_ota_upload.bb

clang-format --dry-run --Werror \
  main/*.c main/*.h \
  components/node_status/*.c components/node_status/include/*.h \
  components/memory_pattern/*.c components/memory_pattern/include/*.h \
  components/board_display/*.c components/board_display/include/*.h \
  components/wifi_service/*.c components/wifi_service/include/*.h \
  components/wifi_service/wifi_station.h \
  components/ota_service/*.c components/ota_service/include/*.h \
  tests/test_*.c
clj-kondo --lint scripts/ota_upload.bb scripts/validate_ota_files.bb tests/test_ota_upload.bb
cljfmt check scripts/ota_upload.bb scripts/validate_ota_files.bb tests/test_ota_upload.bb
```

主机测试使用 IDF 同一份 cJSON，但不模拟 FreeRTOS 调度、真实 SPI/DMA、像素目视、无线驱动事件时序或断电。实际更新证据另见 [TEST_RESULTS.md](TEST_RESULTS.md)。上游 cJSON 曾在 Apple SDK 产生 6 条 `sprintf` deprecated 警告；没有修改/屏蔽第三方源码，不等于第三方依赖安全审计。

纯逻辑 Clang 分析、IDF 编译及历史 GCC `-fanalyzer` 记录在私有证据中；历史分析只属于当时源码。当前两个 OTA ELF 不含 ST7789 构造符，关闭/扫描回归 ELF 不含 OTA 私密输入或 HTTPS 入口。

## 运行行为与尚未验证项

- 心跳约每秒采样，经长度4队列发送值拷贝；接收任务独占状态、约5秒发布JSON，3秒无有效样本标记stalled。两任务优先级2、栈4096字节。
- LCD任务独占句柄；确认ILI9341后固定方向持续提交，未知/不稳定/其他ID停止LCD，不轮换驱动。日志不能代替目视验收。
- STA自动加载应用内凭据，美国US/1–11/MANUAL；不初始化旧NVS，Wi-Fi/PHY/DHCP持久化禁用。旧0.1.4每次UART配网模式不能用于当前固件。
- HTTPS8443只读GET不要求Bearer；上传POST要求独立Bearer，服务端证书必须校验。HTTP200不代表健康，须检查JSON的online/healthy/confirmed及内容hash/运行槽。
- 新应用只写非运行槽，确认成功后才VALID。真实回退故障注入、受控断电、新十分钟长测、真实掉线恢复、WPA3细节、完整8MiB RAM仍未验收。
- 历史构建/结果：[首次烧录](FIRST_FLASH.md)、[PSRAM](PSRAM_PROBE.md)、[LCD](LCD_PROBE.md)、[扫描](WIFI_SCAN.md)、[旧UART配网](WIFI_STA.md)、[自动连接](WIFI_AUTO.md)。旧长测不能用作0.1.7长测结论。
