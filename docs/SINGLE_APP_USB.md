# 单应用分区＋USB更新／恢复（0.3.0 factory，已部署验收）

用户准确批准新应用SHA256 `6da2c426a0c82b54466e4508ea96e72d27751964a284963f92ad88ed1eb7a82a`／980176字节／擦写[0x10000,0x100000)，及新表SHA256 `7c33ad72c9d1933019ac793032f93a01d599206b2610a7d4dcf41219f72ec881`／3072字节／擦写[0x8000,0x9000)，已按顺序各一次USB写入、全片核验后正常启动。无重试/OTA POST/bootloader/NVS/control/eFuse写入；两项许可已耗尽。

**当前设备运行0.3.0-single-usb／factory 0x10000，2.625MiB应用分区。** 一次正常RTS启动只读180.203650秒：PSRAM128KiB六模式、init_stage=ready align_mod8=0 selftest=PASS、固定ILI9341帧1→851、心跳日志0→178；8.384秒ONLINE、8.444秒TCP2323就绪、12.574秒首个healthy记录。仅联网前有一次等待中的unhealthy记录，之后34个健康记录正常。版本/USB/USB only/IP/TCP/nc各一次，无自动eval/整段help；未发UART/TCP测试表达式、未调用accept_*。LCD提交不是目视。

写前新同板/8安全字段/实际完整4MiB旧0.2.4基线及旧引导摘要通过；应用/表FFpadding、全部补集（含bootloader/NVS/reserved/free/完整旧B字节/旧数据）和写后8字段及实际完整4MiB预期通过。**全部Flash/security核验在最后180秒正常启动前，之后只有UART只读采集及本地解析。** 最新 `.local/flash/single-usb-0.3.0/post-protection/current-0.3.0-expected.bin` 是重建且实际完整SDK匹配，非新双读；旧0.2.4基线不再代表当前表/应用。

先读检查点 `execution-CONTINUE.md`／`execution-resume.json`／acceptance-summary／final-boot-summary／post-protection/summary以及两个usb-*-write-attempt。原冻结candidate/CONTINUE/resume仍为PENDING历史，SHA256SUMS/app/ELF/sdkconfig/source不改。UART已释放，无后台监控或更多硬件任务。

源码默认 `SINGLE_APP_USB_ENABLE=y`；Lisp定义仍RAM，未新增持久化加载器、BT/音频/SD/GPIO扩展。USB迁移/正常启动通过不等于实际写回旧版恢复、掉电或新UART/TCP交互回归已验收。

## 分区

新表 `partitions.single.csv`：

| 名称 | 类型 | 地址范围（不含末端） | 大小／用途 |
| --- | --- | --- | --- |
| nvs | data/nvs | [0x9000,0xc000) | 原12KiB，不初始化或写入 |
| reserved | data/custom0x40 | [0xc000,0xe000) | 原8KiB OTA控制字节保留，不擦除、不再作OTA控制 |
| free | data/custom0x40 | [0xe000,0x10000) | 原8KiB，内容不动 |
| factory | app/factory | [0x10000,0x2b0000) | **0x2a0000＝2,752,512字节／2.625MiB**，合并原A/B |
| flash | data/custom0x40 | [0x2b0000,0x2c0000) | 原64KiB，内容不动 |
| js_code | data/custom0x40 | [0x2c0000,0x3c0000) | 原1MiB，内容不动 |
| storage | data/custom0x40 | [0x3c0000,0x400000) | 原256KiB，内容不动 |

只有一份完整应用，没有inactive slot、PENDING/VALID确认或板上旧应用回退。硬件watchdog仍保留；异常重启不等于镜像回退。

容量扩大不是擦除全部2.625MiB的许可。每次只批准实际镜像及扇区向上对齐范围；应用尾部或旧B字节可能暂时仍在Flash，但已没有独立备用分区/回退语义，不能据此承诺恢复能力。

## 运行功能与诊断

- 原固定ILI9341、PSRAM8字节对齐/LispBM owner/启动实际求值probe不变；probe成功仍静默。
- UART115200和无认证TCP2323保留；联网后一次IP/端口/nc提示。不发自动UART/TCP测试表达式。
- 启动增加一行 `Firmware: USB only`；单槽配置不报告OTA8443。
- 默认组件集合不包含 `ota_service`、HTTPS/HTTP server；不链接OTA token/cert/key，不提供 `/ota`、`/status` 或 `/health`。Wi-Fi凭据仍按原私有文件嵌入、只用RAM配置，不改NVS。
- 单槽保留PSRAM启动结果、近期心跳、LCD提交、REPL owner/probe/TCP监听及STA在线状态的健康诊断。状态任务既有stack日志行追加 `runtime_health=starting/healthy/unhealthy; firmware_update=USB`，不新增LCD周期信息或额外成功刷屏。
- 健康诊断没有Flash/确认/复位副作用；启动分配失败会报告，仍可运行的任务保留用于诊断。断网恢复时健康可恢复，但硬件故障和实际恢复未在本版测试。
- `cmake/update_mode.cmake` 拒绝单槽＋旧OTA/HTTPS/回退、不匹配分区，以及启用secure boot、Flash加密或anti-rollback的组合。供电、DIO40MHz/PSRAM40MHz、MEMW、GPIO等原规则不变，默认bootloader禁用1.8→1.9V boost。

LispBM并不隔离所有原生驱动风险；单槽选择的是电脑恢复，而不是认为固件不会再出错。

## 离线构建

使用新目录，不覆盖已部署/冻结构建。完整版本仅需要原Wi-Fi凭据文件，不传OTA私钥/token：

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-single-usb-full \
  -DSDKCONFIG="$PWD/.local/sdkconfig.single-usb-full" \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;tests/sdkconfig.psram.defaults;tests/sdkconfig.lcd.defaults;tests/sdkconfig.lisp-repl.defaults;tests/sdkconfig.wifi-sta.defaults;tests/sdkconfig.wifi-repl.defaults" \
  -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt \
  -DCCACHE_ENABLE=0 -DPROJECT_VER=0.3.0-single-usb build
```

构建输出建议的 `idf.py flash`、`@flash_args`、460800写入命令不能执行。新BIN/ELF/OBJ含Wi-Fi秘密，只留0700目录的私有 `.local/`，不提交或分享。完整Flash参考还保留旧B内的OTA token/TLS私钥等历史秘密；不能因新app不链接OTA密钥就声称Flash已清除它们。

历史OTA组件、`partitions.ota.csv`及mock保留，**只作显式离线回归**。由于IDF在读取CONFIG前展开依赖，历史配置必须额外选择 `-DCOMPONENTS="main;ota_service"`，并在defaults最后追加 `tests/sdkconfig.ota.defaults`；缺少组件选择时构建明确拒绝。该回归用原私有TLS/token，不能冒充单槽镜像或再次上传许可。

## 已完成的离线验证

- 默认64位与额外value32/pointer64模型各 **96 C ASan/UBSan测试通过**，新增3个纯健康状态及1个USB banner用例。后者仍不是Xtensa运行模型。
- **28 Python测试通过**：原15＋4个单槽分区／官方生成器／MD5／保护范围＋9个CMake配置guard。
- **4个历史OTA uploader mock通过**；不执行真实网络/串口。
- 完整STA/TCP、UART REPL、默认关闭、被动扫描共4种单槽构建，以及显式历史OTA共5种配置通过。
- 完整单槽镜像初测 `0xef4d0`＝980176字节，剩 `0x1b0b30`＝1772336字节（约1.69MiB）；最终准确hash/大小以私有冻结candidate为准。ELF保留Lisp/TCP/健康函数，未见OTA服务、HTTP server、上传写入或OTA密钥符号。IDF内部仍可能包含 `app_update` 组件的运行分区查询等基础依赖，不声称整个组件从SDK消失。
- 自有C严格编译/clang-format及纯逻辑分析检查；保留上游LispBM已有4个unused/longjmp警告、主机cJSON deprecated和刻意word32模型转换警告，不改上游或屏蔽警告。
- 初次单槽构建有已移出组件的未知Kconfig符号警告，已通过将OTA选择移到main并删除无效HTTPS默认项修正；历史回归最初暴露CONFIG不能决定早期依赖／alias尚未创建，已改为显式组件＋optional dependency并通过新配置重建。全是本地主机构建问题，无设备操作或OTA重试。

## 首次迁移与后续恢复边界

### 首次迁移已完成（下列为当时流程，权限已消费）

最初“接受，开始吧”只允许离线实现；随后对上述准确hash/范围另行回复“批准”，才执行两次USB写入。下面不是可重放的权限或新操作任务。

1. 冻结准确的新应用、分区表、源码/配置和校验记录，再提出单独的hash/范围/单次USB写入请求。
2. 新只读核对同板身份、8安全字段和实际最新4MiB基线；当前参考仍是 `.local/flash/boot-info-2026-11-10/post-protection/current-0.2.4-expected.bin`，重建且此前实读SDK匹配，**不是新双读备份**。
3. 优先保留当前已知固定SDK的bootloader，不为取消回退而无条件重写引导。已检查 `bootloader_utility_get_selected_boot_partition()` 在无data/ota分区时直接返回factory，不读写旧OTA记录；这是源码证据，不是本版已实测启动兼容性。写前须核对当前引导的实际摘要/配置，若不匹配则阻断而非盲刷。
4. 首次迁移预计只需新应用和分区表的两个明确USB范围；在下载模式内完成且两者都核验后才允许正常启动，避免表/应用不配套时启动。实际顺序、镜像hash、扇区范围和一次性marker另存获批计划；没有通用flash、整片erase、eFuse/NVS/control更新或失败自动重试。
5. 保护引导、NVS/reserved/free/旧数据及全部批准范围外内容。然后只读正常启动采集，确认单槽版本、PSRAM/LCD/probe/TCP/IP及健康诊断。交互验收仍需另外明确许可。

### 后续单槽更新

通常只更新factory应用实际范围，不改表/引导/保留数据。新程序无法联网时，电脑经FTDI B/UART进入ROM下载模式，用已冻结的兼容单槽应用恢复；旧版本也必须匹配当前表和硬件配置。USB人工恢复及断电失败处理尚未实测，不保证任意故障可恢复。

**恢复当前旧双槽0.2.4不只是重刷一个app**：旧应用带双槽OTA服务，不能在新factory表中直接当作兼容恢复镜像。恢复须配套旧分区表、A/B内容及需要恢复的控制记录，明确所有被新单槽应用覆盖的范围，并重新批准；不自动整片写回旧快照。

最新检查点为 `.local/flash/single-usb-0.3.0/`；当前事实以execution-CONTINUE/execution-resume和acceptance-summary等执行记录为准。原candidate/CONTINUE/resume的PENDING冻结历史不改，原0.2.4及所有旧冻结产物也不改。
