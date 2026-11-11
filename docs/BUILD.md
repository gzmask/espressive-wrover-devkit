# 构建、测试与工具链

## 当前0.5.1：帮助改进配置已部署

`.local/build-help-candidate` / `PROJECT_VER=0.5.1-help-usb`已准确批准并唯一USB应用写入；实际完整4MiB保护及180.183秒只读启动通过，help/URL交互未测。主机两模型各150项、28 Python/4历史mock、full及UART离线构建通过。当前事实与最新参考见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。

## 历史0.5.0：正式URL loader配置已部署后被替换

正式目录`.local/build-app-url-candidate`，显式`PROJECT_VER=0.5.0-app-url-usb`，APP/URL/TCP/Wi-Fi及CA/主机名/证书日期检查启用；1194400字节镜像SHA256 `5a8bb890f611dfb274f9b4a5d284654ace2307db994483652c9acdb287b57eea`，准确批准后唯一USB应用写入66.2秒。147源码构建前后/冻结归档一致，SDK头/footer/ELF和配置通过；两主机模型149/149、28Python/4历史mock、六回归配置及正式构建通过。

新同板/8字段/实际完整0.4.0基线及旧引导、写后全部补集/完整4MiB保护通过，之后180.101秒只读启动健康。未发送URL/demo命令，真实HTTPS加载未测；全部写权限耗尽，UART释放。当前参考`app-url-0.5.0/post-protection/current-0.5.0-expected.bin`为重建/实际SDK匹配，非新双读。冻结PENDING/0次保留历史，实际execution-*另存，详见 [APP_URL_LOADING.md](APP_URL_LOADING.md)。

## 历史URL加载开发版（0.5.0-app-url-dev，后来正式候选已部署）

`LISP_APP_URL_ENABLE=n`默认关闭，追加`tests/sdkconfig.app-url.defaults`启用。新独立`.local/build-app-url-{full,uart,disabled,off,scan,legacy}`六配置编译通过；full/uart启用URL，另外四项关闭，legacy显式历史双槽。两主机模型各149项、28Python/4历史mock通过；不改0.4.0候选/任何冻结目录，没有设备访问。精确命令及TLS/时钟要求见 [APP_URL_LOADING.md](APP_URL_LOADING.md)。

该离线开发阶段板上仍0.4.0、无URL接口；后续正式0.5.0部署见顶节。根默认版本仍0.3.0。构建不是新hash的写入/交互授权，不执行工具输出建议的flash命令。

## 历史0.4.0：正式USB应用层配置（已被0.5.0替换）

准确批准后，`.local/build-lisp-app-candidate` / `PROJECT_VER=0.4.0-lisp-app-usb` / `LISP_APP_ENABLE=y`的989280字节镜像唯一USB写入54.1秒；沿用0.3.0表/旧引导，无表/NVS/旧控制/eFuse变化或重试/恢复。新同板/8字段/实际完整0.3.0基线及写后完整4MiB/补集保护通过，之后180.057秒正常启动只读通过；实际probe/PSRAM128KiB六模式/ILI9341帧/心跳/联网健康正常。实际应用切换/绘图/退出和新UART/TCP交互仍未测试，详见 [LISP_APPS.md](LISP_APPS.md)。

全部写权限耗尽，UART释放；当前私有参考`.local/flash/lisp-app-0.4.0/post-protection/current-0.4.0-expected.bin`为重建/实际SDK匹配，非新双读。冻结候选PENDING/0次是原始历史，执行记录另存，不改任何旧档案。构建不是写入/交互许可，不覆盖已部署构建。

## 历史开发阶段（0.4.0-lisp-app-dev，后来按正式候选部署）

追加`tests/sdkconfig.lisp-app.defaults`启用默认关闭的`LISP_APP_ENABLE`；两个固定PSRAM画布共300KiB，正常退出恢复终端。两主机模型各128项、28Python/4历史mock、六配置编译与格式/纯逻辑分析通过。新目录`.local/build-lisp-app-{full,uart,disabled,off,scan,legacy}`，不覆盖旧部署构建；full开发镜像989280字节，非冻结候选。详细命令及限制见 [LISP_APPS.md](LISP_APPS.md)。

该开发阶段没有设备访问或写入授权；后续准确批准的正式USB写入及只读启动见顶节。根默认版本仍0.3.0，开发配置显式指定`PROJECT_VER=0.4.0-lisp-app-dev`，板上正式配置为0.4.0-lisp-app-usb；不要仅凭版本标签判断源码/设备是否匹配。

## 历史0.3.0：首次单槽USB迁移（应用已被0.4.0替换）

默认 `SINGLE_APP_USB_ENABLE=y`／`partitions.single.csv`，factory2.625MiB；网络仅保留原TCP REPL，OTA/HTTPS/镜像回退关闭。新独立 build-single-usb-{full,off,scan,uart} 和显式legacy回归均通过，主机两模型各96项、28Python/4历史mock通过。完整构建命令、约1.69MiB余量及首次迁移/恢复边界见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。

用户另行准确批准后，应用＋表各一次USB迁移及180.203秒正常启动只读验收通过；当前是0.3.0 factory，全部写权限耗尽。构建本身不访问设备；以下旧构建/profile仅历史。新单槽不传OTA密钥/token，不能执行旧OTA上传或工具建议的通用flash。历史OTA回归必须显式 `-DCOMPONENTS="main;ota_service"` 并在defaults最后追加 `tests/sdkconfig.ota.defaults`。

## 历史安静启动与连接提示（0.2.4 B/VALID）

新增repl_boot_info纯格式器；当前源码两模型各92 C ASan/UBSan通过，实际启动probe仍运行但成功静默。版本/IP/TCP2323/nc命令/OTA8443由原owner显示一次，不自动输出help。主机交互验收helper默认拒绝运行，后续启动观察只读，无自动UART/TCP eval。

新回归目录build-boot-info-{off,scan,uart-ota,without-ota}及最终build-boot-info-candidate共五配置通过；冻结.local/flash/boot-info-2026-11-10/已准确批准并唯一POST部署B，180.146秒只读验收通过，新启动信息各一次、无自动eval；A0.2.3保留，全部写权限耗尽。下述0.2.3/冻结资料保留历史，不改归档。见 [WIFI_REPL.md](WIFI_REPL.md)。

## 历史TCP REPL（0.2.3，原A已部分覆盖）

REPL去除证书/token，改TCP2323/nc；`WIFI_REPL_ENABLE`默认n，依赖Lisp+Wi-Fi station，不依赖OTA。旧HTTPS/repl及公开客户端移除；OTA原TLS/Bearer不改。两模型各88 C ASan/UBSan、15Python、4OTA mock、格式/纯逻辑分析通过。build-tcp-repl-{check,off,scan,uart-ota,without-ota}五个新目录编译通过，without-ota构建不传任何cert/key/token，但不能当作可持续OTA候选刷入。

0.2.3用新独立 `.local/build-tcp-repl-candidate`、原OTA密钥/rollback配置，准确批准后已唯一POST部署A/VALID，24项实际TCP含nc/12UART/正常180.049秒通过，B0.2.2保留；冻结旧档案不改，写权限已耗尽。见 [WIFI_REPL.md](WIFI_REPL.md)。

## 当前部署与构建配置

设备实际运行 **`0.5.1-help-usb` / factory 0x10000**；应用唯一USB写入66.2秒，180.183秒正常启动只读通过，无UART/TCP/URL/demo测试输入。单槽没有OTA VALID状态或HTTPS服务；本版LCD未独立目视、应用切屏未交互验收，见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。全部写入许可已消耗，构建不是再次烧录授权。

根 `CMakeLists.txt` 的默认 `PROJECT_VER` 已改为 **0.3.0-single-usb**；单槽USB模式默认启用，可选PSRAM/LCD/Wi-Fi/Lisp仍默认关闭，OTA仅显式历史回归。默认构建不等于上板配置，当前设备只接受经新准确批准的USB固件更新，历史双槽OTA不再可用。新增 `LISP_REPL_ENABLE` 也默认关闭；LispBM0.2.0曾初始化失败回退；0.2.1修复后实机启动/UART已通过，见 [LISP_REPL.md](LISP_REPL.md)。

| 配置 | 构建目录 | 特征 |
| --- | --- | --- |
| 已部署 bootstrap | `.local/build-ota-bootstrap` | 0.1.6、PSRAM/LCD/STA/HTTPS、回退 bootloader、双槽表 |
| 历史 Wi-Fi 版本 | `.local/build-ota-wifi` | 0.1.7、B已被0.2.2替换，冻结文件保留 |
| 历史 UART REPL＋OTA | `.local/build-lisp-ota-aligned` | 0.2.1、已被新A替换，冻结保留 |
| 历史 HTTPS REPL＋OTA | `.local/build-wifi-repl-candidate` | 0.2.2、已被新B替换，冻结保留 |
| 历史 TCP REPL＋OTA | `.local/build-tcp-repl-candidate` | 0.2.3、原A已部分覆盖，不再备用 |
| 历史安静启动＋TCP/OTA | `.local/build-boot-info-candidate` | 0.2.4、旧B字节仍保留但不再有独立槽；冻结不改 |
| 历史单槽USB迁移 | `.local/build-single-usb-full` | 0.3.0、应用已替换；表/引导/档案保留 |
| 历史单槽USB＋TCP＋Lisp应用层 | `.local/build-lisp-app-candidate` | 0.4.0、已被0.5.0替换，冻结不改 |
| 历史单槽USB＋TCP＋应用/URL loader | `.local/build-app-url-candidate` | 0.5.0、已被0.5.1替换 |
| 当前单槽USB＋帮助改进 | `.local/build-help-candidate` | 0.5.1、唯一USB＋只读启动通过，help/URL交互未测 |
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

产物为该目录的 `wrover_radio_node.bin`。默认配置生成`partitions.single.csv`，单factory容量2.625MiB，与当前单槽布局一致；但默认关闭功能的应用**不是板上full配置**，不因表一致就获得写入许可。旧`partitions.ota.csv`/A、B各0x150000只属显式历史回归，不能覆盖当前实物表。

**不执行构建输出建议的 `idf.py flash` / `@flash_args`，不整片擦除。** 本次特定引导/表迁移已获准且已执行，不能作为未来通用 flash 许可。写入和恢复见 [FLASH_AND_RECOVERY.md](FLASH_AND_RECOVERY.md)。

## 历史双槽OTA配置与私有输入（当前0.5.0不使用）

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
- 当前A已VALID；未来新镜像应为非运行B，但仍须新hash/范围批准（会替换现存备用0.1.7）。只上传包含对应 OTA 健康确认逻辑的配置；项目名/IDF 匹配不代表任意默认镜像能完成确认。
- 当前布局现代校验通过；多个 `data/ota` 的错误属于迁移前 Espruino 表历史，不是当前布局错误。

## LispBM第一阶段构建历史（0.2.0失败，0.2.1功能通过并保留A备用）

2026-11-10另获准确批准的 `0.2.0-lisp-repl-ota` 已真实一次Wi-Fi写A，但初始化失败并自动回退到B；见 [LISP_REPL.md](LISP_REPL.md)。既有离线测试不构成硬件功能通过；新旧写入授权已消费。

- `.local/build-lisp-repl/`：PSRAM/LCD/UART REPL，无 Wi-Fi/HTTPS/OTA 服务，不可直接上传。
- `.local/build-lisp-off-check/`：默认关闭回归，不依赖/链接 LispBM。
- `.local/build-lisp-ota-check/`：STA/HTTPS/回退/REPL组合，仅编译；包含既有私有输入，不是批准候选。
- `.local/build-lisp-diagnostic-check/`：PSRAM/LCD/扫描，REPL关闭，保留原诊断画面。

0.2.1修复使用另一组新目录 `.local/build-lisp-ota-aligned/`（完整OTA组合）、`.local/build-lisp-aligned-{off,scan,dev}/`；四配置离线编译通过。冻结 `.local/flash/lisp-2026-11-10-aligned/` 已准确批准并唯一POST部署A，UART/正常启动通过、目视待反馈；许可已消费。

只使用新目录，未覆盖已部署或冻结目录。锁定 LispBM commit、配置、命令和许可证边界见 [LISP_REPL.md](LISP_REPL.md)。标准主机测试新增9项纯文本/输入测试（共54项）；加 `-DENABLE_LISPBM_TESTS=ON` 后当前共74项，包括真实解释器、初始化阶段故障/恢复、启动probe及端到端测试，不是设备验收。额外的value32/pointer64低地址模型与Xtensa对齐编译检查见LISP_REPL.md。

## Wi-Fi REPL（0.2.2已部署B/VALID）

复用8443的认证POST/repl，无新裸TCP监听/WebSocket/UI；默认 `WIFI_REPL_ENABLE=n`。新目录 `.local/build-wifi-repl-candidate/`（0.2.2-wifi-repl-ota）、`build-wifi-repl-{off,scan,uart-ota}/` 四配置通过。冻结 `.local/flash/wifi-repl-2026-11-10/` 已经准确续批并唯一实际POST部署B，A0.2.1保留；验收在renewed-approval/，冻结PENDING原文不改，不是新许可。命令/边界见 [WIFI_REPL.md](WIFI_REPL.md)。

此原型默认主机60项，加真实LispBM81项，额外32位值模型81项；`bb tests/test_wifi_repl.bb` 为纯mock，不能代替真实HTTP/TLS/FreeRTOS邮箱验收。

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
  components/lcd_terminal/*.c components/lcd_terminal/include/*.h \
  components/lisp_repl/*.c components/lisp_repl/include/*.h \
  components/lispbm_runtime/port/*.h \
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
