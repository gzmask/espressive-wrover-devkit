# 单应用分区＋USB更新／恢复（当前0.5.8 factory）

## 当前0.5.8：LCD生命周期，仅应用更新

准确批准SHA256 `d0f8998c6a35e231084a1ea979213655fb41cd7d963f6ff71124890bd0089fc2`／1199600字节／`[0x10000,0x135000)`／293扇区后，新同板/8字段/actual完整0.5.7基线与旧引导预检通过，新marker唯一USB应用写入66.5秒；app/FFpadding/全部补集/8字段/actual完整4MiB预期匹配。无写重试/恢复/OTA/引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.120秒只读通过：factory/0.5.8/冻结ELF前缀600353088、2140ms ready/probe PASS、PSRAM六模式、两次ILI9341 ID/帧1→1351/心跳0→177、ONLINE8500ms/TCP8560ms、34条healthy。该部署启动阶段没有UART/TCP表达式、URL/demo或LCD/SD交互；随后独立获请求的一个TCP连接完成一轮LCD关屏/恢复，关屏保持3.5秒后REPL可用、RAM定义跨周期保留，最终on。卡检测/插拔、SD读写和独立目视仍未验收，SD驱动未实现。

最新参考 `.local/flash/lcd-lifecycle-0.5.8/post-protection/current-0.5.8-expected.bin` 由本次actual SDK匹配0.5.7参考＋准确应用/FFpadding重建并actual完整4MiB匹配，非新双读，含旧B秘密；旧0.5.7及更早全片不再代表当前应用。全部Flash/security核验在最后只读启动前；随后另记TCP RAM生命周期测试，未再次核验Flash/security。新记录先读authorization、usb-app-write-attempt、preflight/summary、app-write-summary、post-protection/summary、final-boot-summary、acceptance-summary、execution-CONTINUE/execution-final-resume；原candidate/CONTINUE/resume/PENDING和38冻结文件不改。

全部写权限耗尽，UART/TCP释放，无后台监控。新写须新准确hash/范围批准、新同板/8字段/最新actual完整0.5.8基线和新marker，单次USB，不自动重试/恢复/OTA/引导/表/NVS/eFuse变化。LCD/SD交互另请；使用时必须先关屏确认off再插卡，启动/运行LCD时不要插拔SD，详见 [LCD生命周期](LCD_LIFECYCLE.md)。

## 历史0.5.7：DMA双条带流水线，仅应用更新

准确批准SHA256 `499b106100c441fc274305ac9b1cc912091bb31e5ec86311c58b83d3d6ae56da`／1197712字节／`[0x10000,0x135000)`／293扇区后，新同板/8字段/actual完整0.5.6基线及旧引导预检通过，新marker唯一USB应用写入66.5秒。app/FFpadding/全部补集/8字段/actual完整4MiB保护通过，无写重试/OTA/恢复或引导/表/NVS/旧控制/eFuse写入。

一次正常RTS启动180.138秒只读通过：factory/0.5.7/冻结ELF前缀dcdce295b、2139ms ready/probe PASS、PSRAM六模式、固定ILI9341帧1→1351/心跳0→177、联网后34条healthy。该阶段无UART/TCP表达式或URL/demo；随后另记一次TCP连接、一次Tiny2D URL加载、两组各50绘制回调并自行退出/恢复终端。整帧墙钟均值Tiny2D 182.330→148.104ms、空present 170.006→140.517ms，非物理LCD FPS，见 [DMA流水线](DMA_PIPELINE.md)。

最新私有`.local/flash/dma-0.5.7/post-protection/current-0.5.7-expected.bin`由新actual SDK匹配0.5.6参考＋准确app/FFpadding重建并actual完整4MiB匹配，非新双读，含旧B秘密；旧0.5.6及更早全片不再适用。全部Flash/security检查在最后只读启动前，之后只TCP RAM诊断未再读。冻结PENDING及SHA256SUMS不改，执行验收/诊断另记。UART/TCP已释放、写授权耗尽；新写须新准确hash/范围、新同板8字段及最新actual完整0.5.7基线、新marker，不自动重试/恢复/OTA/引导/表/NVS/eFuse变化。

## 历史0.5.6：条带转换优化，仅应用更新

准确批准SHA256 `b6eb17035f6ef25c6f21fa4cd015f121742983b562edb4bc21170dc1d61cb550`／1197216字节／`[0x10000,0x135000)`／293扇区。首次预检检测到下载模式但无sync、**应用写入0**；用户重插后独立新同板/8字段/实际完整0.5.5基线及旧引导预检通过，新marker唯一USB应用写入66.4秒，写后app/FFpadding/全部补集/8字段/实际完整4MiB匹配；无USB写重试/OTA/恢复或引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.198秒只读通过：factory/0.5.6/冻结ELF前缀9f0d25ef8、2139ms ready/probe PASS、PSRAM六模式、ILI9341帧1→1051/心跳0→177、ONLINE8509ms/TCP8569ms、34条healthy。随后一次TCP连接实际URL加载Tiny2D并完成两组各50回调设备侧计时，均自行退出；无UART打开、额外复位或Flash/security访问，结果见 [计时诊断](PERF_DIAGNOSTICS.md)。

当时私有`.local/flash/strip-0.5.6/post-protection/current-0.5.6-expected.bin`由actual SDK匹配0.5.5参考＋新app/FFpadding重建并actual完整4MiB匹配，非新双读；后续已被0.5.7替换，不再代表当前应用。所有Flash/security核验在部署只读启动前，后续RAM诊断未再读。冻结PENDING、首次失败和旧execution-CONTINUE均保留，新记录见execution-CONTINUE-replug；写权限耗尽，UART/TCP释放。新写须新准确hash/范围、同板8字段、最新actual完整0.5.7基线和新marker。

## 历史0.5.5：设备侧计时诊断，仅应用更新

准确批准SHA256 `ff6fb7033d5671d83d4217c053cff68064aeb46ae1fab42e44fef0be493c2593`／1197120字节／`[0x10000,0x135000)`／293扇区后，唯一USB应用写入66.4秒。新同板/8字段/实际完整0.5.4基线与旧引导、写后app/FFpadding/全部补集/8字段/实际完整4MiB均匹配；无OTA/重试/恢复或引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.073秒只读通过：factory/0.5.5/冻结ELF前缀91f024584、2139ms ready/probe PASS、PSRAM六模式、ILI9341帧1→1051/心跳0→177、ONLINE8509ms/TCP8569ms、34条healthy。该次启动验收无UART/TCP表达式或URL/demo；随后一次TCP连接实际URL加载Tiny2D并完成两组各50回调设备侧计时，均自行退出；无UART打开、额外复位或Flash/security访问。结果见 [计时诊断](PERF_DIAGNOSTICS.md)。

当时私有`.local/flash/perf-0.5.5/post-protection/current-0.5.5-expected.bin`由actual SDK匹配0.5.4参考＋新app/FFpadding重建并actual完整4MiB匹配，非新双读；后续已被0.5.6替换，不再代表当前应用。所有Flash/security核验在部署只读启动前，后续RAM诊断未再读Flash/security。冻结PENDING历史不改，执行记录另存；写权限耗尽，UART/TCP释放。后续新写边界与当前基线以首节0.5.7为准。

## 历史0.5.4：清屏与矩形优化，仅应用更新

准确批准SHA256 `5d876c17b549ea7bb1c94b9616a89244741c33b6800d7e38703c3fa551c91f3d`／1194800字节／`[0x10000,0x134000)`／292扇区后，唯一USB应用写入66.3秒。新同板/8字段/实际完整0.5.3基线及旧引导，写后应用/FFpadding/所有补集/8字段/实际完整4MiB均通过；无OTA/重试/恢复或引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.025秒只读通过：factory/0.5.4/冻结ELF前缀d6aba6efe、2138ms ready/probe PASS、PSRAM六模式、ILI9341帧1→1051/心跳0→177、ONLINE8508ms/TCP8568ms、34条healthy。该次启动验收没有UART/TCP表达式或URL/demo，复位清除RAM定义；后续按用户请求进行本版六组RAM profiling，Tiny2D约5.07逻辑回调/秒，较0.5.3本次对比5.32未见提升，不是物理LCD FPS，详见 [应用层](LISP_APPS.md)。

当时私有`.local/flash/raster-0.5.4/post-protection/current-0.5.4-expected.bin`由actual SDK匹配0.5.3参考＋新app/FFpadding重建并actual完整4MiB匹配，非新双读；已被0.5.5替换，不再代表当前应用。所有Flash/security保护在部署只读启动前，后续用户请求了RAM profiling，没有再读Flash/security。冻结candidate/CONTINUE/resume及SHA256SUMS不改，实际authorization/marker/执行与验收另存。写权限耗尽，UART/TCP释放；后续新写边界和当前基线以首节0.5.7为准。

## 历史0.5.3：100ms目标周期，仅应用更新

准确批准SHA256 `f357c1b37a6ca67e982d7da0bfc51fb250e0b8547adb24096ab03563a1c4bfe8`／1194736字节／`[0x10000,0x134000)`／292扇区后，唯一USB factory应用写入66.1秒；新同板/8字段/实际完整0.5.2基线与旧引导、写后应用/FFpadding/所有补集/8字段及实际完整4MiB预期通过。无OTA/重试/恢复或引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.146秒只读通过：factory/0.5.3/冻结ELF前缀8e1379d54、2138ms ready/probe PASS、PSRAM六模式、两次9341/ILI9341帧1→1051/心跳0→177、ONLINE8508ms/TCP8568ms、34条healthy。终端submitted_frames 1@2718ms→1051@179288ms约5.95次/秒，含重复帧，不是动画或目视FPS。该次启动验收未发送UART/TCP表达式、URL或demo；复位清除RAM定义。随后用户请求并允许串口打开可能启动，实际一次启动后通过URL加载Tiny2D并完成RAM profiling：Tiny2D约5.32逻辑回调/秒、空present对照约5.61次/秒，不是物理LCD FPS；两组自行退出。

当时私有`.local/flash/period-100ms-0.5.3/post-protection/current-0.5.3-expected.bin`由actual SDK匹配0.5.2参考＋新app/FFpadding重建并actual完整4MiB匹配，非新双读；已被0.5.4替换，不再代表当前应用。全部Flash/security检查在部署的180秒启动前；此后用户请求了RAM profiling，没有再读Flash/security。冻结candidate/CONTINUE/resume及SHA256SUMS不改，实际authorization/marker/执行与验收另存。冻结CONTINUE的0.5.1基线模板错误见独立`.local/100ms-candidate-erratum.txt`，实际使用和检查的是0.5.2。写权限耗尽，UART释放；后续新写边界与当前基线以上节0.5.4为准。

## 历史0.5.2：帧预算修复，仅应用更新

准确批准SHA256 `09148ca37003a3280a50143f386fa6d2390f23819076f648ca6a8ed3d07d216e`／1194720字节／`[0x10000,0x134000)`／292扇区后，唯一factory USB应用写入66.3秒。写前新同板/8字段/实际完整最新0.5.1基线及保留旧引导，写后应用/FFpadding/全部补集/8字段/实际完整4MiB预期均通过；无引导/表/NVS/reserved/eFuse/OTA写入、重试或恢复。

一次正常RTS启动180.185秒只读通过：factory/0.5.2/冻结ELF前缀9284be7bd、2138ms ready/8字节对齐/probe PASS、PSRAM128KiB六模式、两次一致9341/单固定ILI9341帧1→851、心跳0→177、ONLINE8518ms/TCP8568ms、34条healthy。未发送UART/TCP表达式、URL或demo，实际新动画速率/目视未验收；旧profiling不能挪作修复后的性能。RAM定义已清除。

当时私有`.local/flash/cadence-0.5.2/post-protection/current-0.5.2-expected.bin`由actual SDK匹配0.5.1参考＋准确应用/FFpadding重建并actual完整4MiB匹配，非新双读；部署保护在0.5.2最后启动前。后续用户明确请求后已做Tiny2D RAM profiling；再后来0.5.3写入已替换应用，因此0.5.2全片不再代表当前设备。最新写入边界及基线以上节为准。

## 历史0.5.1：帮助改进，仅应用更新

准确批准SHA256 `5c017c7f6e8bfa64675eb62e3f6673bcb24651ff3ce269dcda7f09172e3b6e86`／1194720字节／`[0x10000,0x134000)`／292扇区后，一次USB应用写入66.2秒。写前新同板/8字段/实际完整0.5.0基线及旧引导、写后应用/FFpadding/全部补集/8字段/实际完整4MiB保护通过；无引导/表/NVS/旧控制/eFuse写入、OTA、重试或恢复。

一次正常RTS启动只读180.183秒：factory/0.5.1/冻结ELF前缀、ready/probe/PSRAM六模式/ILI9341帧1→851/心跳0→177、联网后34条healthy通过。没有UART/TCP表达式、URL或demo；新帮助交互和LCD目视未验收。重启清除RAM定义；串口已释放，无后台监控。

当时私有 `.local/flash/help-0.5.1/post-protection/current-0.5.1-expected.bin` 重建且实际完整SDK匹配，非新双读；该次全部Flash/security核验在最后启动前。后续0.5.1实际URL加载Tiny2D做过RAM profiling；再后来0.5.2更新已替换应用，0.5.1全片不再代表当前状态。旧B秘密仍保留但不是备用槽；后续新写须使用当前0.5.7实际基线并另获准确批准。

## 历史0.5.0：仅应用更新，URL loader已编译启用

准确批准`5a8bb890f611dfb274f9b4a5d284654ace2307db994483652c9acdb287b57eea`／1194400字节／`[0x10000,0x134000)`／292扇区后，唯一USB写入66.2秒；无引导/表/NVS/旧控制/eFuse写入、OTA、重试或恢复。新同板/8字段/实际完整0.4.0基线和旧引导、写后应用/FFpadding/全部补集/8字段/实际完整4MiB保护通过。

180.101秒正常RTS启动只读：factory/0.5.0/冻结ELF前缀、ready/probe/PSRAM六模式/ILI9341帧1→851/心跳0→178、ONLINE8508ms/TCP8568ms、联网后34条healthy通过，无URL loader启动错误。没有URL/demo/UART/TCP表达式，真实在线加载和LCD目视/应用退出未验收；RAM定义已清除。出站HTTPS仅加载应用到RAM，不是固件OTA或HTTPS服务，见 [URL加载](APP_URL_LOADING.md)。

当时私有`.local/flash/app-url-0.5.0/post-protection/current-0.5.0-expected.bin`为重建且实际完整SDK匹配，非新双读，现不代表当前应用；全部保护在最终启动前，此后只有UART采集/本地整理。旧0.4.0全片已不代表当前应用，旧档案不改，旧B秘密/字节保留不等于备用槽。全部写权限耗尽，UART释放，无后台监控；交互另批。后续新固件须新准确hash/实际factory扇区范围/同板8字段/最新实际完整基线/新marker单次USB，不重试或自动恢复。

## 历史0.4.0：仅应用更新，沿用单槽表与旧引导

准确批准SHA256 `564c97b824af015d0b475399ce75abd192aa103f37f9a3a30c5f089bf3b68d0b`／989280字节／`[0x10000,0x102000)`/242扇区后，一次USB应用写入54.1秒，无表/bootloader/NVS/旧控制/eFuse写入或重试/恢复。写前新同板/8字段/实际完整0.3.0基线和旧引导，写后应用/FFpadding/全部补集/8字段及实际完整4MiB预期均匹配。所有Flash/security检查在最后启动前。

**180.057秒正常启动只读通过**：0.4.0/factory/冻结ELF前缀、实际probe PASS/align_mod8=0、PSRAM128KiB六模式、固定ILI9341帧1→851、心跳0→177；ONLINE8417ms/TCP8487ms，12577ms起34条healthy（最后177577ms），仅联网等待中6577ms一次unhealthy。版本/USB/USB only/IP/TCP/nc各一次，无自动eval/help，没有UART/TCP测试表达式或demo。Lisp前台应用已启用，但实际切换/绘图/退出和新LCD目视仍未验收，见 [LISP_APPS.md](LISP_APPS.md)。

最新私有`.local/flash/lisp-app-0.4.0/post-protection/current-0.4.0-expected.bin`是准确应用＋实际SDK匹配0.3.0参考＋FFpadding重建并实际完整SDK匹配，非新双读备份。旧0.3.0全片不再代表当前应用；旧B秘密与字节保留不是备用槽/秘密清除。所有保护在最后180秒之前，此后仅UART读取及本地解析，未再读Flash/HTTPS。

当前检查点`.local/flash/lisp-app-0.4.0/`，先读execution-CONTINUE/execution-resume/acceptance-summary/final-boot-summary/post-protection/summary和authorization/usb-app-write-attempt；冻结candidate/resume/CONTINUE旧PENDING/0次不改，不能重用权限。全部写权限耗尽，UART释放，无后台监控，交互另需批准。

## 历史0.3.0：首次应用＋表迁移（旧应用已替换）

用户准确批准新应用SHA256 `6da2c426a0c82b54466e4508ea96e72d27751964a284963f92ad88ed1eb7a82a`／980176字节／擦写[0x10000,0x100000)，及新表SHA256 `7c33ad72c9d1933019ac793032f93a01d599206b2610a7d4dcf41219f72ec881`／3072字节／擦写[0x8000,0x9000)，已按顺序各一次USB写入、全片核验后正常启动。无重试/OTA POST/bootloader/NVS/control/eFuse写入；两项许可已耗尽。

**当时设备运行0.3.0-single-usb／factory 0x10000，2.625MiB应用分区；当前版本见首节。** 一次正常RTS启动只读180.203650秒：PSRAM128KiB六模式、init_stage=ready align_mod8=0 selftest=PASS、固定ILI9341帧1→851、心跳日志0→178；8.384秒ONLINE、8.444秒TCP2323就绪、12.574秒首个healthy记录。仅联网前有一次等待中的unhealthy记录，之后34个健康记录正常。版本/USB/USB only/IP/TCP/nc各一次，无自动eval/整段help；未发UART/TCP测试表达式、未调用accept_*。LCD提交不是目视。

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

最新检查点为 `.local/flash/dma-0.5.7/`；当前事实以execution-CONTINUE、acceptance-summary及target-timing等执行记录为准。原candidate/CONTINUE/resume的PENDING冻结历史不改，0.5.6及更旧冻结产物也不改。
