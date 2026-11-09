# Wi-Fi 刷机：双槽 HTTPS OTA（0.1.7已通过真实Wi-Fi部署并验收）

**当前0.1.7-ota-wifi / B（0x160000），明确VALID；阶段已关闭。** 用户批准新四项hash后一次USB安装0.1.6入口；180.047秒启动及TLS状态通过、用户确认LCD无翻转，随后**仅一次真正HTTPS POST上传1014048字节的0.1.7（HTTP200），无重传、无UART写B**。设备软件重启进入B，38686ms完成30秒连续健康确认。270.037秒过渡采集包含完整升级和B首次启动，之后保护核验通过，再正常RTS复位180.024秒、真实固定证书认证状态及镜像内容hash/槽/online/healthy/VALID再次匹配。

Wi-Fi后B应用及3补集digest、8安全字段、实际完整4MiB重建预期均通过；seq1/seq2均VALID且CRC有效，A控制扇区不变。**保护核验在最终正常B启动前，不声称之后再次读取Flash。** 所有3项USB与一次Wi-Fi写入授权均已消耗，不可自动重刷/恢复。

## 当前接口与认证边界

| 接口 | 当前实现 | 实机证据 |
| --- | --- | --- |
| `GET /status` | 只读，无需Bearer，客户端仍须校验TLS服务端证书 | 已实测版本、内容hash、运行槽、online/healthy/confirmed |
| `GET /health` | 与/status共用同一handler和JSON，无需Bearer | 路由/代码已检查，未单独调用实机验收 |
| `POST /ota` | 必须Bearer；目标槽及镜像hash绑定请求 | 真实唯一POST/HTTP200、重启和VALID通过 |

TLS证书校验的是服务端身份，不等于GET进行了客户端认证。此前GET验收请求携带了Bearer，但handler不校验它。可达局域网的客户端可以读取状态，不向公网开放。GET在成功序列化时返回HTTP200，即使healthy=false；必须判断JSON，不把200当健康。

## 已完成的首次安装流程

1. 首次经 USB 安装已冻结的 0.1.6-ota-bootstrap、确定版本的 ESP-IDF v5.5.6 回退 bootloader 和双槽表，并初始化 OTA 控制记录。
2. 设备用应用内Wi-Fi凭据自动连接，开启HTTPS8443：只读GET无需Bearer，上传POST必须Bearer；没有配网、文件浏览或任意命令服务。
3. 电脑使用固定证书验证服务端，用独立随机 32 字节 token 认证，只向非运行槽上传应用。没有明文 HTTP、`-k` 或外网暴露/端口映射。
4. 上传长度受限，检查目标槽、项目名、芯片、IDF 版本、整个上传体 SHA-256 及 SDK 镜像完整性；只有全部通过才切换启动记录并重启。中断不覆盖运行槽。元数据提交失败后拒绝再次上传，须停止检查，不盲目重试。
5. 新固件的 PSRAM 自检、近期心跳/队列投递、LCD 提交、联网及 HTTPS 服务连续健康 30 秒才确认。待验证启动 120 秒仍未确认时请求重启，由新 bootloader 回退；初始化任务分配失败会重启，启动挂死由 watchdog 处理。**尚未实测回退/受控断电，不能声称所有故障均能自动恢复。**

不是固件签名或安全启动方案，eFuse、Flash 加密及 anti-rollback 均不启用。用于可信局域网，不是公开互联网设备管理服务。

## 分区迁移与保护

旧布局只有 factory 和一个 OTA 槽，且 js_code/storage 被错误标为额外 data/ota。不能直接当成可持续更新的标准双槽布局；旧 bootloader 的准确版本/回退能力也未能确认。

新表 `partitions.ota.csv` **不移动任何分区地址/大小**：

| 用途 | 地址 / 大小 | 变化 |
| --- | --- | --- |
| NVS | 0x9000 / 0x3000 | 内容不动、不初始化 |
| OTA 控制 | 0xc000 / 0x2000 | 初始化并允许 SDK 启动/切换/确认状态更新 |
| free | 0xe000 / 0x2000 | 内容不动 |
| OTA A | 0x10000 / 0x150000 | 由 factory 改标 ota_0，首次放 0.1.6 |
| OTA B | 0x160000 / 0x150000 | 由旧 ota_0 改标 ota_1，首次 Wi-Fi 放 0.1.7 |
| flash/js_code/storage | 0x2b0000 起到 0x400000 | 内容不动；后两者改为 custom data 子类型 |

写前完整基线中B整槽经离线核实为全FF，本次首次放入0.1.7。此前“可能覆盖旧备用Espruino”是保守风险告知，不是该槽存在旧应用的证据；原始Espruino整片双备份仍留在私有目录。表迁移、bootloader 更新或供电中断可能需要 USB 恢复，不保证迁移过程可抗任意断电。

本次已批准并各单次执行的USB范围为 bootloader+表 `[0x1000,0x9000)`、OTA 控制 `[0xc000,0xe000)`、A 应用 `[0x10000,0x108000)`；一次实际Wi-Fi上传B `[0x160000,0x258000)`。准确四项 hash 只见私有冻结请求，不能复用此前应用专用授权。不会写 0x0–0x1000、NVS、free 或旧用户数据，不执行通用 `idf.py flash` 或整片擦除。

## 私有材料与构建

`.local/ota/keys/` 中的 `token.bin`（随机 32 字节）、`server-cert.pem`（wrover.local 自签名证书）、`server-key.pem` 均为当前用户所有的 0600 普通文件。Wi-Fi 仍从原两行文件链接。**应用/ELF/生成汇编/目标文件含 Wi-Fi 凭据、token 和 TLS 私钥，不可提交或分享。** 目录 0700，禁用 ccache。不把值放到源码/sdkconfig/命令行，不发布原凭据文件或 token 的内容/摘要。

两版共用同一套证书/key/token；不要随意覆盖或重新生成，换密钥需协调客户端信任与恢复路径。构建源归档不含这些外部私密输入，不能独立重建同一镜像。

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-ota-bootstrap \
  -DSDKCONFIG="$PWD/.local/sdkconfig.ota-bootstrap" \
  -DSDKCONFIG_DEFAULTS="$PWD/sdkconfig.defaults;$PWD/tests/sdkconfig.psram.defaults;$PWD/tests/sdkconfig.lcd.defaults;$PWD/tests/sdkconfig.wifi-sta.defaults;$PWD/tests/sdkconfig.ota.defaults" \
  -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt \
  -DOTA_TOKEN_FILE="$PWD/.local/ota/keys/token.bin" \
  -DOTA_CERT_FILE="$PWD/.local/ota/keys/server-cert.pem" \
  -DOTA_KEY_FILE="$PWD/.local/ota/keys/server-key.pem" \
  -DCCACHE_ENABLE=0 -DPROJECT_VER=0.1.6-ota-bootstrap -DIDF_TARGET=esp32 build
```

上述是已执行bootstrap的构建记录，不要覆盖已验证目录。0.1.7使用另一目录`.local/build-ota-wifi`、独立sdkconfig及`PROJECT_VER=0.1.7-ota-wifi`构建。未来版本使用新目录/新版本，并保留对应OTA健康确认与回退配置；任意默认关闭配置不一定能维持后续OTA入口或完成确认。保留 ESP32 revision1/MEMW、DIO/40 MHz、4 MB、PSRAM40 MHz；新 bootloader 明确不执行 SDK 默认的 1.8→1.9V LDO boost，不做电压/GPIO 试探。回退需要与该表匹配的新 bootloader，不能仅给旧板上传应用后假定具备回退能力。

## 上传工具（再次更新仍需新准确hash/范围批准）

```sh
umask 077
bb scripts/ota_upload.bb \
  --ip DEVICE_IPV4 --image /path/to/frozen-app.bin \
  --cert .local/ota/keys/server-cert.pem --token .local/ota/keys/token.bin \
  --sha256 EXACT_APPROVED_FILE_SHA256 --slot-address 0x10000 \
  --evidence-dir .local/evidence/ota/unique-upload-directory
```

当前运行B，下一次经新批准的更新目标应为非运行A（`0x10000`）；刚完成的历史0.1.7上传目标才是B（`0x160000`）。示例不是新写入许可，不重放已消费的hash或证据目录。

实际只发 **一次 POST**，目标地址也随请求明确绑定，避免预检查后设备换槽导致写错授权范围。token 只进临时 0600 curl 配置，用完删除，不进进程参数。固定证书和 HTTPS 主机名经 curl connect-to 对应当前 IP，无须改 DNS/路由或安装全局 CA。

`--sha256`及`X-Image-SHA256`是整个BIN文件的SHA-256，包含末尾附加摘要；`GET /status`中的`image_sha256`是SDK镜像摘要，对应BIN末尾32字节。两者不是同一个值，工具分别验证，不把文件hash直接与状态字段比较。

工具不会将ACK当作完成；只做后续只读查询，直到目标镜像内容摘要、运行槽、在线/健康、明确VALID状态均匹配。丢失 ACK 不重发 POST。失败停止，不自动刷回/重试；IP 改变时先从 LCD/私有串口或路由器确认新 IP，再只读核实，不能因旧 IP 不通就重传。

## LCD 周期翻转处理

用户报告旧 0.1.5 整屏约两分钟翻转。180秒串口采集开头观察到一次复位，之后只识别/运行 ILI9341；用户随后反馈不再翻转，**原始根因尚未确认，不能称故障已修复**。

源码中的遗留 UNKNOWN-ID 双驱动轮换已移除：必须两次一致 ID4=9341，仅初始化 ILI9341 一次、固定 swapXY=true/mirror=false,false；未知/不稳定/其他型号停止 LCD，不自动尝试 ST7789。失效时 LCD 健康为 false，因此新 OTA 不会靠错误驱动刷帧而确认有效。原引脚、时钟、供电不变。新增严格失败关闭测试先 RED 后 GREEN，两个 OTA ELF 均无 ST7789 构造符。

0.1.6完成180秒采集后用户确认无翻转，再执行一次Wi-Fi更新。0.1.7两次启动均仅ILI9341，首次帧1→221、最终帧1→171、栈最低3992字节；这些日志不等于助手完成新B全程目视检查，也不证明所有原始物理原因已被排除。

## 已验证 / 尚未验证

- 45 项 C ASan/UBSan，15 项 Python（含官方分区生成器 4 项），上传工具 4 个 mock 场景通过。
- mock 覆盖成功、丢 ACK 后只查询、未确认当前镜像拒传、401 不重试；**不是实机网络测试**。
- bootstrap、Wi-Fi 目标、默认关闭、扫描回归四配置构建通过，无 warning/error；Babashka REPL、clj-kondo/cljfmt、C 格式检查通过。
- 分区官方验证、MD5 往返、地址/大小保持、唯一控制区、双等大 OTA 槽；seed seq1/VALID CRC 与 SDK otatool 一致。
- **真实Wi-Fi OTA PASS**：唯一POST/HTTP200，B运行镜像内容hash/版本/槽匹配，SW_CPU_RESET后30秒健康确认VALID，最终正常复位后仍VALID。69.1秒为客户端完整流程（预查/上传/重启/确认），不是纯上传用时。
- **硬件保护 PASS**：写前身份/当前完整基线/8安全字段；写后B和全部补集、实际控制记录及完整4MiB重建预期匹配。NVS/free/旧用户数据不变，A及引导/表在Wi-Fi阶段不变；不改eFuse。当前快照是重建且实际SDK摘要匹配，不是新双读。
- **双B启动 PASS**：PSRAM128KiB六模式、两轮9341、US/1–11/MANUAL、一次嵌入加载、首次关联/DHCP、心跳/LCD/HTTPS持续进度、无故障日志。首次B持续约220秒LCD跨度，最终正常B独立180.024秒采集。不是十分钟长测。
- **尚未验证** 实际失败回退/故障注入、受控断电、完整8MiB PSRAM、新十分钟soak、真实Wi-Fi掉线恢复及WPA3细节。自动确认已实测不代表回退已实测。

已部署冻结产物与关闭检查点：`.local/flash/ota-2026-11-08-lcd-fixed/`。无 SD、OTA 分区/bootloader 网络升级、Agent、BLE、LoRa、uLisp。不自动 commit/push。
