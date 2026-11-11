# ESP-WROVER-KIT bring-up

ESP-WROVER-KIT v4.1 / 经典 ESP32 的 ESP-IDF 固件：FreeRTOS、PSRAM、ILI9341、自动 Wi-Fi、USB固件更新与 RAM-only LispBM REPL。

## 当前设备：0.5.7 双条带 DMA 流水线（factory已部署）

准确批准SHA256 `499b106100c441fc274305ac9b1cc912091bb31e5ec86311c58b83d3d6ae56da`／1,197,712字节／`[0x10000,0x135000)`／293扇区后，新完整预检通过，唯一USB应用写入66.5秒。app/FFpadding/全部补集/8字段/actual完整4MiB保护通过，无写重试/OTA/恢复或引导/表/NVS/eFuse写入；一次180.138秒正常只读启动healthy。

随后相同方法实测两组各50绘制回调：Tiny2D LCD整帧墙钟均值从0.5.6的182.330ms降至148.104ms（约18.8%），空present从170.006ms降至140.517ms（约17.3%）；均自行退出并恢复终端。流水线增加10KiB内部RAM，SPI仍10MHz、队列深度1；默认关闭，板上明确启用。范围重叠的计时不能相加，不是纯CPU或物理LCD FPS，100ms目标仍未达到。详见 [DMA流水线](docs/DMA_PIPELINE.md)。

最新私有参考为`.local/flash/dma-0.5.7/post-protection/current-0.5.7-expected.bin`，重建且actual完整SDK匹配，非新双读；所有Flash/security检查在只读启动前，后续TCP RAM诊断未再读。写权限耗尽，UART/TCP释放；候选基于未提交工作树/base commit `7baa764`，源码尚未提交。

## 历史0.5.6 条带转换优化（已被0.5.7替换）

准确批准SHA256 `b6eb17035f6ef25c6f21fa4cd015f121742983b562edb4bc21170dc1d61cb550`／1,197,216字节／`[0x10000,0x135000)`／293扇区。首次预检串口无sync，**写入0**；用户重插后新同板/8字段/实际完整0.5.5基线与旧引导通过，唯一USB应用写入66.4秒，app/FFpadding/全部补集/8字段/实际完整4MiB保护通过，无写重试/OTA/恢复或引导/表/NVS/eFuse写入。180.198秒正常启动只读通过。

随后同方法实际运行Tiny2D与空present各50回调：Tiny2D LCD条带转换平均从0.5.5的66.006ms降至50.233ms，LCD整帧从197.100ms降至182.330ms；提交与等待仍约131.783ms。设备侧墙钟含调度和诊断开销，不是纯CPU或物理LCD FPS；不同任务范围重叠，不能跨行相加。详见 [计时诊断](docs/PERF_DIAGNOSTICS.md)。

当时私有`.local/flash/strip-0.5.6/post-protection/current-0.5.6-expected.bin`重建且实际完整SDK匹配，非新双读，后续已被0.5.7替换；所有Flash/security检查在只读启动前，此后诊断没有再读Flash/security。写权限耗尽，UART/TCP释放；候选基于未提交工作树及base commit `59eed25`，详见 [单槽USB](docs/SINGLE_APP_USB.md)。

## 历史0.5.5 前台应用计时诊断（已被0.5.6替换）

准确批准SHA256 `ff6fb7033d5671d83d4217c053cff68064aeb46ae1fab42e44fef0be493c2593`／1,197,120字节／`[0x10000,0x135000)`／293扇区后，唯一USB应用写入66.4秒。新同板/8字段/实际完整0.5.4基线及旧引导、写后app/FFpadding/全部补集/8字段和实际完整4MiB均通过；无OTA/重试/恢复或引导/表/NVS/eFuse写入。一次正常启动180.073秒只读通过，factory/0.5.5/冻结ELF前缀91f024584/probe PASS、PSRAM六模式、ILI9341帧1→1051、联网后34条healthy。

随后按用户请求用一次TCP连接实际运行Tiny2D与空present各50回调并读取设备侧计时：Tiny2D平均LCD整帧197.100ms、条带转换66.006ms、提交与等待130.817ms、画布draw/复制32.216ms；这些是含调度和仪表开销的墙钟，不同任务区间重叠，不能跨行相加，非物理LCD FPS。数据见 [计时诊断](docs/PERF_DIAGNOSTICS.md)；随后条带优化已随0.5.6部署。

当时私有`.local/flash/perf-0.5.5/post-protection/current-0.5.5-expected.bin`重建且实际完整SDK匹配，非新双读；已被0.5.6替换，不再代表当前全片。写权限耗尽，UART/TCP释放；候选构建时基于未提交工作树及base commit `59eed25`。详情见 [单槽USB](docs/SINGLE_APP_USB.md)。

## 历史0.5.4 清屏与矩形优化（已被0.5.5替换）

准确批准SHA256 `5d876c17b549ea7bb1c94b9616a89244741c33b6800d7e38703c3fa551c91f3d`／1,194,800字节／`[0x10000,0x134000)`／292扇区后，唯一USB应用写入66.3秒。新同板/8字段/实际完整0.5.3基线和旧引导、写后app/FFpadding/全部补集/8字段及实际完整4MiB预期均通过；无OTA、重试、恢复或引导/表/NVS/eFuse写入。

一次正常启动180.025秒只读通过：factory/0.5.4/冻结ELF前缀d6aba6efe、2138ms ready/probe PASS、PSRAM六模式、ILI9341帧1→1051/心跳0→177、联网后34条healthy。该次验收没有UART/TCP表达式或URL/demo；后续按用户请求RAM复测，Tiny2D绘制标记段由0.5.3的75.24ms降至59.69ms，但逻辑周期从187.98ms变为197.29ms，未观察到整体回调率提升；物理LCD FPS未测。当时私有参考`.local/flash/raster-0.5.4/post-protection/current-0.5.4-expected.bin`重建且实际完整SDK匹配，非新双读；后续已被0.5.5替换，不再代表当前全片。写权限耗尽，UART/TCP释放；候选构建时基于未提交工作树及base commit `59eed25`，详见 [单槽USB](docs/SINGLE_APP_USB.md)。

## 历史0.5.3 应用与LCD目标周期100ms（已被0.5.4替换）

准确批准SHA256 `f357c1b37a6ca67e982d7da0bfc51fb250e0b8547adb24096ab03563a1c4bfe8`／1,194,736字节／`[0x10000,0x134000)`／292扇区后，唯一USB应用写入66.1秒。新同板/8字段/实际完整0.5.2基线及旧引导、写后app/FFpadding/所有补集/8字段/实际完整4MiB均通过；无引导/表/NVS/eFuse/OTA写入、重试或恢复。

一次正常启动180.146秒只读通过：factory/0.5.3/冻结ELF前缀8e1379d54、ready/probe PASS、PSRAM六模式、ILI9341帧1→1051/心跳0→177、联网后34条healthy。终端状态下submitted_frames从1@2718ms到1051@179288ms，约5.95次/秒，含重复帧，不是实际动画或目视FPS；100ms目标不保证10FPS。该次启动验收没有UART/TCP表达式、URL或demo，复位清除RAM定义；随后按用户请求通过URL加载Tiny2D并完成RAM profiling，约5.32逻辑回调/秒，空present对照约5.61次/秒，均非物理LCD FPS。后续组件标记对照提示清屏与矩形耗时较明显，见 [应用层](docs/LISP_APPS.md)。

当时私有`.local/flash/period-100ms-0.5.3/post-protection/current-0.5.3-expected.bin`重建且实际完整SDK匹配，非新双读；0.5.3部署核验后曾按请求做RAM profiling，后续已被0.5.4替换，不再是当前完整基线。全部写权限耗尽，UART释放，无后台监控。构建时来自未提交工作树及base commit `3811321`，完整源码快照已冻结；源码后来提交`59eed25`，不是候选构建时提交。详情见 [单槽USB](docs/SINGLE_APP_USB.md)。

## 历史0.5.2 帧预算修复（已被0.5.3替换）

准确批准SHA256 `09148ca37003a3280a50143f386fa6d2390f23819076f648ca6a8ed3d07d216e`／1,194,720字节／`[0x10000,0x134000)`／292扇区后，唯一USB应用写入66.3秒。新同板/8安全字段/实际完整0.5.1基线与旧引导，写后应用/FFpadding/全部补集/8字段及实际完整4MiB预期通过；没有引导/表/NVS/eFuse/OTA写入、重试或恢复。

一次正常启动180.185秒只读通过：factory/0.5.2/冻结ELF前缀9284be7bd、2138ms ready/probe PASS、PSRAM六模式、ILI9341帧1→851/心跳0→177、联网后34条healthy。该次启动验收没有UART/TCP表达式、URL或demo；后续按用户请求实测Tiny2D逻辑回调约4.94Hz（旧版约3.01Hz），不是LCD目视FPS；TinyGL未重测。刷后RAM定义已清除，后续profile已重载Tiny2D，两个demo均无需修改其`.lisp`。

帧调度已从“回调结束＋200ms”改为“回调开始＋200ms”，只等待剩余预算，超预算不额外等满200ms、不补帧；64步求值批次、GC、LCD/SPI均不变。源码提交`3811321`已推送；详情见 [应用层](docs/LISP_APPS.md)。当时私有参考`cadence-0.5.2/post-protection/current-0.5.2-expected.bin`重建且实际完整SDK匹配，非新双读；该次Flash/security核验在0.5.2最终启动前，后续按请求进行了RAM profiling，现已被0.5.3替换。

## 历史0.5.1 帮助改进（已被0.5.2替换）

准确批准后，`0.5.1-help-usb`／SHA256 `5c017c7f6e8bfa64675eb62e3f6673bcb24651ff3ce269dcda7f09172e3b6e86`／1,194,720字节／`[0x10000,0x134000)`唯一USB应用写入66.2秒。写前同板/8字段/实际完整0.5.0基线和旧引导通过；写后应用/FFpadding、全部补集、8字段及实际完整4MiB匹配，无其他区域写入、重试或恢复。

一次正常启动只读180.183秒：factory/冻结ELF前缀、实际probe PASS、PSRAM六模式、ILI9341帧1→851、心跳0→177、联网后34条healthy。没有发送UART/TCP表达式或URL/demo命令，`:help`实际交互和LCD目视尚未验收。RAM定义随重启清除；全部写权限耗尽。最新私有参考 `help-0.5.1/post-protection/current-0.5.1-expected.bin` 为重建且实际完整SDK匹配，非新双读；全部Flash/security核验在最后启动前。

## 历史0.5.0 URL → RAM 应用加载（已被0.5.1替换）

准确批准SHA256 `5a8bb890f611dfb274f9b4a5d284654ace2307db994483652c9acdb287b57eea`／1,194,400字节／`[0x10000,0x134000)`／292扇区后，**唯一USB应用写入66.2秒**，保留表/引导/NVS/reserved/free/旧B和范围外字节，无OTA/重试/恢复。写前新同板/8字段/实际完整0.4.0基线和旧引导，写后应用/FFpadding/全部补集/8字段/实际完整4MiB均匹配。

**180.101秒正常启动只读通过**：factory/版本/冻结ELF前缀、2138ms ready/align_mod8=0/probe PASS、PSRAM128KiB六模式、ILI9341帧1→851/心跳0→178；ONLINE8508ms/TCP8568ms，12648ms起34条healthy、最后178648ms，无URL loader启动错误。未发UART/TCP/URL/demo命令，LCD目视与真实在线加载仍未验收。原RAM demo/定义随复位清除，未自动重载。

已启用 `(app-load-url "https://…/app.lisp")`：最多32KiB、后台HTTPS→RAM、原owner加载定义，`:load-status`／`:load-cancel`；成功后仍显式`app-run`。CA/主机名/日期校验，首次请求按需限时校时；不写Flash/NVS、不自动run/重试/重定向，不新增OTA或监听。见 [URL加载](docs/APP_URL_LOADING.md)。

当时私有参考`app-url-0.5.0/post-protection/current-0.5.0-expected.bin`为重建/实际完整SDK匹配，非新双读，现已不代表当前应用；所有Flash/security核验在最终启动前，此后仅UART采集和本地整理。旧0.4.0全片不再代表当前应用，旧B秘密保留不是备用槽或秘密清除。全部写权限耗尽，UART释放，无后台监控，交互另批；不自动commit/push。

## 历史0.4.0 LispBM 应用层（已被0.5.0替换）

以下demo是当时RAM运行记录；本次0.5.0更新已清除，未重新加载。

用户明确请求后，已通过TCP一次加载demo、一次run，9秒后确认应用仍持有屏幕，关闭连接后保留运行；未q/:stop、复位或刷写。实际LCD目视/颜色/FPS和退出返回终端仍待确认，见 [LISP_APPS.md](docs/LISP_APPS.md)。

准确批准后，`0.4.0-lisp-app-usb`／989280字节／SHA256 `564c97b824af015d0b475399ce75abd192aa103f37f9a3a30c5f089bf3b68d0b`，在`[0x10000,0x102000)`/242扇区**一次USB写入54.1秒**。沿用0.3.0表和旧bootloader，无NVS/旧控制/旧数据/范围外写入、OTA、重试或恢复。

- **180.057秒正常启动只读通过**：factory10000/2a0000、版本/冻结ELF前缀、ready/align_mod8=0/实际probe PASS、PSRAM128KiB六模式、固定ILI9341帧1→851、心跳0→177；ONLINE8417ms/TCP8487ms，12577ms起34条healthy。版本/USB/USB only/IP/TCP/nc各一次，无自动help/eval。
- 写前/写后实际完整4MiB与8安全字段、应用/FFpadding/全部保护范围通过，**均在最终正常启动前**；之后只有UART读取与本地解析。最新私有`lisp-app-0.4.0/post-protection/current-0.4.0-expected.bin`是重建/实际SDK匹配，非新双读；旧0.3.0全片已不代表当前应用，旧冻结不改。旧B字节及秘密保留不是备用槽或秘密清除。
- 本版明确启用`LISP_APP_ENABLE=y`：全屏app、init/step/exit、绘图、UART Ctrl-C / TCP :stop/:key X及恢复终端。**该USB只读启动验收阶段未加载demo或发送表达式；后续demo已按明确请求运行并确认逻辑持屏，但显示目视/退出未验收**。定义仍RAM、输入1023字节，尚无文件加载器。示例 [`examples/lisp/demo.lisp`](examples/lisp/demo.lisp)，见 [LISP_APPS.md](docs/LISP_APPS.md)。
- 两模型各128 C ASan/UBSan、28Python/4历史mock、六配置及正式候选构建通过。新写入helper模型/marker mock及2个只读capture mock先通过，不混作实机交互。全部写权限消费，UART释放，无后台监控；后续交互另批、固件须新准确hash/范围批准，不自动BT/DSP/commit/push。

## 历史设备：0.3.0单槽＋USB首次迁移（已被0.4.0应用替换）

用户准确批准后，应用和分区表各一次USB写入完成，无重试/OTA POST。设备运行 **0.3.0-single-usb / factory 0x10000**，应用分区2.625MiB；镜像980176字节，余约1.69MiB。原bootloader、NVS、reserved旧控制、free和旧用户数据及全部范围外Flash保持不变。旧B字节保留但不再是备用槽；不做镜像回退。

- **180.203秒正常启动只读验收通过**：单次启动、实际probe PASS/8字节对齐、PSRAM128KiB六模式、固定ILI9341帧1→851、心跳日志0→178；8.384秒联网/8.444秒TCP就绪，联网后的健康记录均正常。版本/USB/Firmware: USB only/IP/TCP2323/nc各一次，无测试表达式或自动整段help。LCD提交不是独立目视。
- Wi-Fi仍为无认证TCP2323；底层固件用USB更新/恢复，不提供HTTPS8443/OTA/status/health。全部写入权限耗尽，UART释放，无后台监控；本版未跑新UART/TCP交互回归。
- 8安全字段及实际完整4MiB保护核验均在最终启动前；最新私有参考single-usb-0.3.0/post-protection/current-0.3.0-expected.bin是重建/实际SDK匹配，非新双读。旧0.2.4全片预期不再代表当前表/应用，冻结历史不改。两主机模型各96项、28Python、4历史mock及五配置构建通过。详见 [单槽USB](docs/SINGLE_APP_USB.md)。

## 历史设备：安静启动与连接提示（0.2.4 B/VALID，现已迁移）

LCD/串口启动显示版本、IP、TCP2323和`nc IP 2323`；联网后仅报告一次，不自动输出整段help。单次真实健康probe成功静默；批量eval验收脚本默认停用，后续启动观察只读，不自动发送UART/TCP表达式。

- 准确批准后唯一真实OTA POST/HTTP200写1106720字节，38836ms完成30秒健康确认，A0.2.3完整保留备用，无重传/USB镜像写入。
- **正常B启动180.146秒只读观察通过**：版本/IP/端口/nc各一次，无eval结果/远程回显/自动help；帧1→851、心跳日志0→177，最终HTTPS版本/SDKhash/B/healthy/online/VALID匹配。本次没有UART/TCP测试输入，不把0.2.3的交互测试算成本版结果。
- B/padding、3补集、8安全字段、实际seq5/6 VALID/CRC及完整4MiB重建预期匹配，A控制扇区不变；核验在最终B重启前，之后只读HTTPS，非新双读。所有写权限耗尽，UART释放，无后台监控。LCD提交不是助手目视；UART初始connecting行可被SDK日志插入，最终IP/端口/nc在原始日志完整。
- 仍直接`nc DEVICE_IPV4 2323`，无需证书/token/login；OTA原保护不变。当前IP私存boot-info-2026-11-10/final-https/device-ip.txt。两模型各92项和五配置构建通过，见 [WIFI_REPL.md](docs/WIFI_REPL.md)。

## 历史0.2.3无证书/token TCP验收（原A已部分覆盖，不再备用）

直接 **`nc DEVICE_IPV4 2323`**，无需证书、token、登录或专用客户端。原HTTPS `/repl` 已移除，OTA8443原TLS/Bearer保护不变。任何LAN可达客户端均能执行RAM Lisp，仅可信局域网，不公开暴露。

- 准确批准后唯一真实OTA POST/HTTP200写A1106176字节，38806ms完成30秒健康确认；B0.2.2保留备用，无重传/USB写镜像。
- **24项真实明文TCP（含真正nc）＋12项UART、正常A重启180.049秒通过**：3/42/hello、定义/reset/错误恢复、分包/多行/编辑CRLF、NUL/超长拒绝、半句EOF不执行/重连RAM保留/UART busy。LCD帧1→851、心跳日志0→179；本版LCD未独立目视。
- A/padding、3补集/完整B、8安全字段、实读seq5/4 VALID/CRC及完整4MiB重建预期匹配；核验在最终A启动/测试前，之后只读HTTPS，非新双读。全部权限耗尽，UART释放，无后台监控。
- 主机两模型各88项及五配置编译通过，不混作上述设备证据。当前IP私存tcp-repl-2026-11-10/final-https/device-ip.txt，详见 [WIFI_REPL.md](docs/WIFI_REPL.md)。DSP/蓝牙未实现。

## 历史0.2.2 HTTPS REPL验收（原B已被0.2.4替换）

**当前运行 `0.2.2-wifi-repl-ota`，B槽 `0x160000`，VALID/healthy/online；Wi-Fi REPL可用，UART保留。**

- 续批相同准确hash/范围后唯一真实OTA POST/HTTP200，1106880字节；先观察启动/HTTPS就绪再预检，38688ms完成30秒健康确认。之前前置GET阻断的调用POST=0，不是另一次上传。
- 正常B重启独立180.103秒，12项UART＋17项真实认证HTTPS REPL通过（3/42/hello/定义/错误恢复/重置、401/400/409）；帧1→851、心跳日志0→176。LCD提交不是目视证据，本版文字/滚动未独立目视。
- B及padding、3补集、8安全字段、有效seq3/4 VALID控制CRC和完整4MiB重建预期匹配；当前A0.2.1和受保护数据不变。保护核验在最后B重启前，之后只读HTTPS；非新双读备份。
- 所有写权限已耗尽，UART释放、无后台采集。Wi-Fi复用HTTPS8443认证POST/repl；不加Telnet/WebSocket/UI，定义仅RAM。客户端见 [WIFI_REPL.md](docs/WIFI_REPL.md)，DSP/蓝牙未实现。

### 历史0.2.1 A验收（现已被0.2.3替换）

- 新准确批准后唯一一次HTTPS POST/HTTP200写A，1104672字节；8字节对齐和真实(+1 2)==3启动probe通过，未重传或USB写镜像。
- 正常A重启后独立180.052秒，12项UART实测通过：3/42/hello、定义/lambda、清行/退格、错误恢复、RAM重置及定义遗忘；帧1→851、心跳0→179，最终固定证书HTTPS版本/内容hash/槽/健康/VALID匹配。
- A及padding、3补集、8安全字段和完整4MiB重建预期通过；B0.1.7、bootloader/表/NVS/free/旧数据未变。**保护核验在最终A重启/REPL测试之前，之后仅只读HTTPS，不声称再读Flash。**
- 全部写入授权已消费；串口释放，无后台采集。**REPL仅USB串口115200输入，Wi-Fi仅HTTPS状态/OTA，不提供网络REPL**；不是蓝牙键盘，DSP/蓝牙仍未实现。详情 [LISP_REPL.md](docs/LISP_REPL.md)。

## 历史Wi-Fi验收（2026-11-08，原0.1.7 B已被0.2.2替换）

设备曾通过真实Wi-Fi更新到 `0.1.7-ota-wifi` / B / VALID，下述为当时验收记录。

- 首次 USB 安装 `0.1.6-ota-bootstrap`、固定版本的回退 bootloader、双槽分区表和 OTA 控制记录，各写一次。
- 用户确认 LCD 不再翻转后，仅一次 HTTPS POST 更新到 0.1.7，HTTP 200，无重传、无 UART 写 B。新固件软件重启，完成 30 秒连续健康确认。
- 270.037 秒过渡采集包含升级与 B 首次启动；随后另一次正常 RTS 复位、独立 180.024 秒采集及证书校验的 HTTPS 状态检查通过。
- 上传后完整 4 MiB 预期、全部授权范围外 Flash、控制记录 CRC/VALID 和 8 项安全字段通过。**保护核验在最终正常启动前**，不声称之后再次读取 Flash。
- NVS、free 和旧 JS/存储数据未变。写前 B 整槽实际为 `0xFF`，不是已有备用 Espruino 应用；原始 Espruino 整片双备份保留。
- **全部写入授权已消耗**。再次更新必须确认新镜像 hash、目标槽和准确范围；不自动重刷、恢复备份或整片擦除。

## LispBM首次失败与修复历史（2026-11-10）

用户另行批准后，`0.2.0-lisp-repl-ota` 经唯一一次真实 HTTPS POST 写入 A，HTTP200且镜像校验/启动成功；但 **LispBM 初始化失败**，未获健康确认，120秒后自动回退到原 **B / 0.1.7 / VALID**。当时A为ABORTED，B及范围外Flash未变，未重传或改走USB。这份0.2.0已淘汰；当前A已被另获批准的0.2.1替换并VALID，不能再用该失败阶段全片预期核验当前设备。

源码的 UART REPL/40×20 ASCII终端仍默认关闭。0.2.1已离线修正8字节存储对齐，新增分阶段诊断及真实启动求值probe；74项主机测试和额外32位值模型通过，仍不能代替ESP32实机启动验收。一次USB只读同步失败曾阻断上传；用户重插后重新枚举并完成同板/8字段/完整4MiB/BHTTPS预检，才执行已准的唯一POST。0.2.1现已真实运行并通过UART/正常启动验收，未跳过保护检查；LCD目视仍待反馈。详情见 [LISP_REPL.md](docs/LISP_REPL.md)。

## 历史Wi-Fi HTTPS REPL（0.2.2，已替换）

HTTPS8443认证 `POST /repl`，原token/TLS，无新裸TCP监听/WebSocket/浏览器。旧客户端现仅私存 `.local/scripts/wifi_repl_0_2_2.bb`，一行一请求，仅原REPL任务求值，返回纯文本并同步LCD，定义共享RAM。17项实际网络测试通过；协议、客户端命令及限制见 [WIFI_REPL.md](docs/WIFI_REPL.md)。

## 已实现与限制

| 项目 | 当前状态 |
| --- | --- |
| 硬件 | ESP32-D0WD rev1.0，4 MB Flash，40 MHz 晶振，CPU 160 MHz；不是 ESP32-S3 |
| PSRAM | 8 MiB 器件、4 MiB 映射；启动测试及独占 128 KiB 六模式通过，不是完整 8 MiB 测试 |
| LCD | 固定ILI9341 / 320×240、40×20 ASCII终端；两次一致9341才初始化，未知ID停止；提交日志通过，终端目视待反馈 |
| LispBM | USB UART115200＋无认证TCP2323，共享RAM；0.5.0只读启动/probe通过，URL加载/应用切换未交互验收；旧交互仅历史 |
| Wi-Fi | 应用内嵌凭据，启动自动连接；美国 US / 信道 1–11 / MANUAL，仅 2.4 GHz；不写旧 NVS |
| 网络 | 明文TCP2323 REPL无TLS/token；可选出站HTTPS URL loader，无HTTPS8443/status/health/OTA服务 |
| 固件更新 | 单factory2.625MiB，USB更新／电脑恢复；首次应用＋表迁移通过，无板上镜像回退，实际写回恢复尚未验收 |
| 未实现 | DSP/音频、蓝牙键盘/音箱、SD、Agent、BLE、LoRa、uLisp；不提供明文 HTTP、文件管理、任意命令或网络 bootloader/分区表更新 |

原周期翻转的根因尚未确认。已移除旧 UNKNOWN-ID 双驱动轮换，用户确认 bootstrap 无翻转；B 的刷新日志不能冒充助手全程目视观察。

受控断电、真实掉线恢复、WPA3/PMF/SAE 细节、新十分钟长测及完整 8 MiB PSRAM 仍未验收。旧版 660/690 秒结果只属于各自历史版本。

最后发现的 UART 为 FTDI B `/dev/cu.usbserial-21401`，A/JTAG 为 `21400`；重插后必须重新枚举，115200 baud。验收结束时串口已释放，没有持续后台监控。

## 构建与测试

ESP-IDF 固定 **v5.5.6** / commit `880a203ae603d08a94d06aa01e0d6761958e5e9e`。源码默认版本已改为 **0.3.0-single-usb**，默认单槽USB更新模式，可选PSRAM/LCD/Wi-Fi/Lisp仍需明确启用。0.3.0完成单槽迁移，0.4.0启用Lisp应用层，0.5.0启用URL loader，当前板上为0.5.7；源码默认关闭配置不等于板上full配置。

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

URL源码两主机模型各149项C ASan/UBSan、28Python、4历史mock、六配置及正式0.5.0候选编译通过；唯一USB写入/180.101秒正常只读启动通过，真实URL交互未测。0.4.0冻结源码当时各128项及正式候选构建通过，另有唯一USB应用写入和180.057秒factory正常启动只读证据。旧12UART/24TCP交互仅历史，本版不自动eval。mock不替代实机证据。精确构建见 [BUILD.md](docs/BUILD.md)，当前更新边界见 [单槽USB](docs/SINGLE_APP_USB.md)，旧OTA见 [历史OTA](docs/OTA.md)。**不要执行构建输出建议的通用 `idf.py flash` / `@flash_args`。**

凭据、token、TLS 私钥由外部私有文件链接，不放值进源码/sdkconfig/命令行。BIN/ELF/生成汇编/目标文件和当前 Flash 快照含这些秘密，只留私有、Git 忽略的 `.local/`，禁用 ccache。目录 0700，输入/日志 0600，冻结产物 0400。不要分享镜像或重新生成正在使用的 key/token。

## 文档导航

| 文档 | 用途 |
| --- | --- |
| [单槽USB](docs/SINGLE_APP_USB.md) | 当前factory布局、USB更新/恢复、实际迁移验收及边界 |
| [OTA](docs/OTA.md) | 旧双槽HTTPS与历史升级记录；当前单槽不再使用 |
| [构建](docs/BUILD.md) | 固定工具链、默认关闭与 OTA 配置、测试 |
| [LispBM REPL](docs/LISP_REPL.md) | 当前UART/LCD与首次失败/修复历史 |
| [LispBM 应用层](docs/LISP_APPS.md) | 已部署配置；Tiny2D实际运行并退出，LCD目视待验收 |
| [DMA流水线](docs/DMA_PIPELINE.md) | 默认关闭、板上明确启用；正常路径有限实测通过，目视/故障注入未测 |
| [URL 应用加载](docs/APP_URL_LOADING.md) | 已部署配置；HTTPS→RAM／状态／取消，已实际加载Tiny2D，其他边界未测 |
| [蓝牙键盘调研](docs/BLUETOOTH_KEYBOARD.md) | 离线HID Host选型、配对/NVS阻断与最小输入接入方案；尚未实现 |
| [Wi-Fi REPL](docs/WIFI_REPL.md) | 当前无认证TCP2323/nc协议与原owner/RAM边界 |
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
components/ota_service/      历史双槽回归，仅显式选择，当前固件不链接
components/lisp_app/         双画布所有权、裁剪/光栅与控制解析
components/lisp_app_url/     有界URL下载/邮箱、可选出站HTTPS worker
components/lcd_terminal/     40×20 ASCII模型与纯RGB565条带渲染
components/lispbm_runtime/   固定上游解释器及单所有者适配
components/lisp_repl/        RAM运行时、分阶段初始化/probe、UART交互
partitions.single.csv        当前单factory分区（已部署）
partitions.ota.csv           历史双槽回归分区定义
scripts/                    构建输入校验、旧配网工具、单次 OTA 上传器
tests/                      主机测试及独立构建配置
docs/                       当前说明与明确分版的历史记录
```

当前私有检查点 `.local/flash/dma-0.5.7/`；先读execution-CONTINUE、acceptance-summary及`target-timing/analysis.md`。`post-protection/current-0.5.7-expected.bin`是重建且实际完整SDK摘要匹配的4MiB预期，**不是新双读备份**，所有保护在部署只读启动之前。旧0.5.6及更早全片不再代表当前应用；冻结candidate/source/PENDING历史不改，执行验收及后续诊断另存。
