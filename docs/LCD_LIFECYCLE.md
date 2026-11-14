# LCD 生命周期与 SD 卡互斥（0.5.12已部署，有限实卡闭环通过）

当前0.5.12包含LCD生命周期、SD文件API和`app-load-sd`。0.5.11曾有限实测FAT32文件写读/卸载重挂；0.5.12已完成31字节源码和三个原demo（658/1096/2438字节）实卡加载，卸载取卡后恢复LCD，三demo各短运行约5秒并正常q退出。最终LCD on、SD unmounted、卡在板外，端口已释放，无后台应用。它们是正常路径的软件状态/实际SD交互证据，不是LCD独立目视、GPIO电气波形、热插拔、带卡复位或故障注入验收。详见[SD卡API](SD_CARD.md)。

## 强制门控与物理边界

- 只有LCD `off`/`sd-blocked`可取得SD租约；`on`/`starting`/`stopping`/`fault`拒绝挂载。关背光不是`off`：必须由原LCD owner完成当前帧/DMA并拆除panel/IO/SPI，最后交出GPIO21。
- 从挂载前取得租约到成功卸载/释放SDSPI资源期间，`lcd-on`被固件拒绝。普通读写和`app-load-sd`都要求已挂载，不能绕过租约。
- 不确定DMA或SD写/sync/close/清理失败保守隔离；不释放仍可能在使用的资源。`fault`不等于可拔卡的`unmounted`。
- LCD初次启动/每次恢复前多次采样卡检测，检测到卡则`sd-blocked`，但运行时没有可靠持续检测；**不防止物理热插拔，也不能保证带卡复位安全**。

顺序仍必须是：关屏并确认`off` → 插卡 → 挂载/操作 → 卸载确认`unmounted` → 用户取卡 → 开屏。加载源码只求值定义，不自动运行；`app-run`须在取卡/恢复LCD之后。蓝牙A2DP不使用这些LCD GPIO，本功能不是蓝牙驱动。所有固件写权限已耗尽，新写仍须准确hash/范围许可。

## 历史0.5.8有限交互：一轮LCD关闭/恢复通过

用户明确“测试一下”后，一个TCP连接执行一次 `lcd-off`、一次 `lcd-on`：初始on，关屏首个状态查询为off，保持3.5秒后仍off；`(+ 20 22)`和测试RAM定义均返回42。开屏轮询两次starting、第三次on，保持3.5秒后仍on，测试定义仍返回42，最后发送undefine清理。无UART打开/主动复位/重连/重传/Flash/security/NVS/eFuse/URL/demo/SD访问，连接关闭后LCD最终on，无后台任务。

证据另存 `.local/flash/lcd-lifecycle-0.5.8/lifecycle-test/` 的authorization、connect-attempt、各请求/回复/timing和summary；原部署acceptance-summary与冻结档案不改。新会话/故障停止模型及Kondo/cljfmt先通过。这里的on确认owner首帧完成，不是独立目视、GPIO电压波形或卡槽物理检查；仅一轮短测，不证明重复长期启停、DMA故障隔离或SD冲突已全部硬件验收。原始部署只读启动范围与该后续交互分开记录。

## 已部署：SD API与FatFs worker

可选`CONFIG_BOARD_SDCARD_ENABLE`新增Lisp异步mount/unmount/status/read/write/result/error接口，使用SDSPI GPIO2/13/14/15及GPIO21 card-detect；LCD必须off并持有GPIO/SPI租约。单worker串行处理，直接FatFs logical drive（不注册POSIX VFS），1–32字符平面文件名、ASCII文本最大512字节，write会持久创建/截断文件，无自动格式化、目录、二进制或启动自动挂载。源码实现及当前限制见 [SD卡API](SD_CARD.md)。

SD-on/perf、UART和SD-off三配置构建及两种主机模型各214项通过；主机fake不是实卡证据。0.5.11短FAT32写读与0.5.12源码加载/取卡恢复LCD/运行退出已有限实测，不能外推不同卡、长期、热插拔、带卡复位、DMA/SD故障或全部电气顺序。开发镜像不可代替准确批准的冻结镜像。

## 历史0.5.9：LCD help唯一USB部署/保护/只读启动通过

用户准确批准新hash/范围后，离线冻结候选经实际同板/8字段/0.5.8完整基线/旧引导预检通过，唯一USB应用写入与保护通过。新私有检查点`.local/flash/lcd-help-0.5.9-freeze2/`，镜像版本`0.5.9-lcd-help-usb`：

- 1,200,096字节，SHA256 `b40ba185fd4719f3c558d33cf466413b85b42f09cb8b413cb62af4446aa4493a`。
- 镜像`[0x10000,0x134fe0)`；擦写`[0x10000,0x135000)`，293扇区，与0.5.8相同。
- 仅申请一次factory USB应用写入；单槽无板上回退，表/引导/NVS/reserved/free/旧数据和范围外不写。准确新同板/8字段/实际完整0.5.8基线与旧引导预检必须通过；新marker最多一次，不重试/恢复/OTA。
- 基于干净提交`fcf5c9e5ead9aaebccbf22a58b97d98eaed53195`的独立正式构建。181源码文件构建前后/归档/current一致，38冻结文件0400/目录0700、SHA256SUMS和解包审计通过。镜像头/footer/ELF/config/符号/帮助字符串、本地最新0.5.8表/引导/参考、FFpadding/补集模型通过；本地0.5.8参考此前实际完整SDK匹配，但不是本轮新设备观察或双读。
- 两主机模型各185 CTest、28 Python/4历史mock；六配置full/UART/off/probe/serial/legacy和正式候选构建、Kondo0/cljfmt、格式及Clang分析通过。已知上游UBSan对齐诊断保留，不称sanitizer-clean。
- **PASS（写入/保护/启动）** — 准确批准后，实际同板/8字段/完整0.5.8基线与旧引导预检通过，唯一USB应用写入66.6秒；app/FFpadding/全部补集/8字段/actual完整4MiB保护通过。180.133秒正常RTS只读启动factory/0.5.9/ELF前缀562037d04，probe/PSRAM六模式/ILI9341帧1→1351/联网健康34条通过。无写重试/恢复/OTA/引导/表/NVS/eFuse写入。
- **NOT TESTED（交互）** — 此次只读启动未发送`:help-lcd`或LCD/SD命令，没有独立目视；新帮助页板上显示尚未交互验收。SD驱动、插拔和卡读写仍未实现/测试。0.5.8的一轮生命周期交互是历史独立证据，不冒充0.5.9实测。
- 最新完整参考`.local/flash/lcd-help-0.5.9-freeze2/post-protection/current-0.5.9-expected.bin`为本次actual SDK匹配0.5.8本地参考＋准确应用/FFpadding重建并actual完整4MiB匹配，非新双读；全部保护在最后启动前，此后只有UART日志采集/本地解析。写权限耗尽、UART/TCP释放、无后台监控。

## 部署候选准备（随后已批准并部署）

新候选已部署；冻结PENDING历史和SHA256SUMS不改，执行记录另存。准确`:help-lcd`文本经过主机行宽/容量和socket/LispBM模型测试，但0.5.9启动验收没有实际发`:help-lcd`，板上帮助显示/独立目视未验收。LCD/SD命令未执行，SD驱动未实现。

## 历史0.5.8部署记录：唯一USB部署/保护/180秒只读启动通过

用户对下节准确hash/范围回复“批准。”，新专用部署helper/模型先通过，然后新端口20301实际同板/8字段/完整0.5.7基线与旧引导预检通过，新marker唯一USB应用写入66.5秒。准确app/FFpadding/全部补集/8字段及actual完整4MiB保护匹配，无失败/写重试/恢复/OTA或引导/表/NVS/旧控制/eFuse写入。

保护后一次正常RTS启动180.120秒只读通过：factory/0.5.8/冻结ELF前缀600353088、2140ms ready/probe PASS、PSRAM六模式、两次ILI9341 ID/帧1→1351/心跳0→177、ONLINE8500ms/TCP8560ms、34条healthy。日志未出现卡检测启动阻断；这不等于独立验证卡槽、电平波形或物理卡片状态。本轮无UART/TCP表达式、URL/demo/LCD/SD交互或LCD目视，不能把正常显示启动当作关闭/恢复已实测。

新执行记录在 `.local/flash/lcd-lifecycle-0.5.8/`：authorization、usb-app-write-attempt、preflight/summary、app-write-summary、post-protection/summary、final-boot-summary、acceptance-summary、execution-CONTINUE/execution-final-resume。旧candidate/CONTINUE/resume与38冻结文件的PENDING/0次保留历史不改。最新 `post-protection/current-0.5.8-expected.bin` 为本次actual SDK匹配0.5.7参考＋准确应用/FFpadding重建并actual完整4MiB匹配，非新双读；含旧B秘密，旧0.5.7全片不再适用。所有Flash/security检查在最后启动前，此后只UART采集/本地解析，未再读Flash或访问TCP。

部署阶段UART/TCP已释放，无后台监控，全部写权限耗尽。后来LCD生命周期一轮交互见首节；额外测试/SD插拔读写仍需独立请求和验收；本次提交包含生命周期实现、验收文档与后续帮助改进，不重建或覆盖部署镜像。

## 历史候选冻结（随后获准确批准并部署）

用户在新 hash 出现前回复“批准，继续”，只用于离线复核/冻结，不是本镜像的设备写许可。新私有检查点为 `.local/flash/lcd-lifecycle-0.5.8/`，先读 `candidate`、`authorization-request`、`CONTINUE` 和 `resume`：

- 版本：`0.5.8-lcd-lifecycle-usb`；1,199,600 字节。
- SHA256：`d0f8998c6a35e231084a1ea979213655fb41cd7d963f6ff71124890bd0089fc2`。
- 镜像：`[0x10000,0x134df0)`；实际擦写：`[0x10000,0x135000)`，293 个4KiB扇区，与0.5.7相同。
- 仅申请一次 factory USB 应用写入；单槽无板上回退，表/引导/NVS/reserved/free/旧数据和全部范围外不写。启动验收须 SD 槽为空，不自动进行 LCD/SD 交互或插拔测试。
- 独立 `.local/build-lcd-lifecycle-candidate` 基于未提交工作树/base commit `912112bc84e9f7c6b4a072804086ec9eef08287a`，不是从该提交直接构建。181 文件源码快照在构建前后及归档审计时一致；后续候选说明文档另行更新，不改变冻结源码。
- SDK 镜像头/footer/ELF、固定配置/符号、当前0.5.7本地参考/旧引导/表/FFpadding和补集模型通过；该参考此前已 actual 完整 SDK 匹配，本轮未新读设备或新双读。38 冻结文件0400/目录0700及摘要、归档审计通过。
- 两主机模型各185项CTest、28项Python、4历史mock、六种开发配置与新正式候选离线构建、C格式、纯状态/扩展Clang分析通过。已知上游UBSan对齐诊断保留，不声称sanitizer-clean。
- 本轮没有串口枚举/打开、设备网络/复位/Flash/security/NVS/eFuse/SD访问，没有新authorization、attempt或preflight；旧冻结及已部署0.5.7档案不改。新准确批准之后仍须专用helper/模型、新同板/8字段/最新actual完整0.5.7基线和旧引导预检、新marker单次USB，失败停止，不重试/恢复/OTA。

## 帮助说明（0.5.12已包含）

`:help`保留三个LCD API名称，并提供`:help-lcd`入口。新说明页解释请求异步、状态等待、先等off再插卡、取卡后才on、禁止运行中插拔，以及先停止app/URL加载。主帮助与LCD帮助均保持每行小于40字符、总行数不超过19行，并验证TCP回复容量。

`:help-lcd`源码已在`fcf5c9e`提交并进入后续0.5.9与当前0.5.12，0.5.8冻结镜像不含此页。0.5.12另含`:help-sd`和主帮助SD摘要；帮助文本/布局主机测试通过，但没有独立LCD目视验收。

## 使用方式

在 UART 或可信 LAN 的 TCP REPL 中，应用退出且 URL 加载结束后：

```lisp
(lcd-status)  ; on / starting / stopping / off / sd-blocked / fault / disabled
(lcd-off)     ; 接受停止请求返回 t，不代表已停止
(lcd-status)  ; 等到 off 才可插卡并让 SD 驱动接管
(lcd-on)      ; 先停止并释放 SD、取出卡；接受启动请求返回 t
(lcd-status)  ; 等到 on 才表示首帧已成功提交
```

- 必须从 UART/TCP 操作；LCD 关闭后本机屏幕不可作为唯一交互途径。REPL 和终端快照继续在 RAM 更新，重新启动 LCD 后显示最新快照；定义不会因为开关屏幕而清除。
- `t` 仅是请求被接受；异步初始化、DMA、驱动或卡检测仍可能失败。`starting`/`stopping` 时拒绝并发切换。状态 `on` 是软件提交成功，不证明肉眼可见。
- `sd-blocked` 表示启动前卡检测至少一次为低；先取出卡并等待，再显式 `(lcd-on)`。无自动重试。`fault` 表示运行或清理失败，拒绝再次切换/SD 接管，隔离至复位；不能把它当 `off`。`disabled` 代表 Lisp 没有附接后端。
- 绘图应用运行/退出恢复期间及 URL 加载期间拒绝开关；即使应用恢复超时，画布未确认回到终端前也拒绝关屏。关闭或切换中 `(app-run ...)` 不可用。加载的代码和无认证 TCP 客户端也能调用这些 API：现有 REPL 不是安全沙箱，仅用于可信 LAN。
- 当前单槽 USB 配置中，用户有意 `off` 或 `sd-blocked` 不算 LCD 故障；启动/停止过渡、`fault` 和运行时未按时提交帧仍不健康。串口/TCP 的健康门控不因此关闭。历史 OTA 配置仍要求 LCD 实际运行并有近期帧，不放宽其确认门控。

## 电气边界：先停屏，再插卡

原厂 v4.1 原理图：LCD D/C 与 SD 卡检测通过 R167 共用 GPIO21，检测线上有上拉和卡槽开关。实现只针对 **原始接线且 R167 在位**：LCD 初次启动及每次恢复前，先把 GPIO21 配为输入并多次采样；只要有低电平就不启动 LCD。采样不是对卡槽、焊接、卡或开关状态的可靠自检，也不能消除采样后插卡的竞态。

**LCD 启动中或运行中不要插拔 SD 卡。** 先执行 `(lcd-off)`，确认 `off`，再插卡。以后使用 SD 驱动时先卸载/停止并确认释放，取卡后才能 `(lcd-on)`；复位也会尝试启动 LCD，启动前应先取卡。若卡槽检测异常或 R167 被移除，该软件检查不再能保证防冲突。禁止带电改板；LCD 与 SD 同时使用及拆 R167 均不在本实现范围。

除GPIO21检测共线外，SPI2 host也由LCD和SDSPI互斥使用；SD当前使用GPIO2/13/14/15，涉及启动绑带/JTAG约束，不能因软件租约存在就忽略电气、供电和启动行为。当前仅正常流程短挂载/文件交互通过，不保证热插拔或带卡启动安全。

## 实现与资源所有权

`components/board_display/display_lifecycle.c` 是纯状态与清理序列，`board_display.c` 的原 LCD owner 独占实际操作：

1. 收到停止请求后，不再发起新帧，等待正在处理的完整帧及其最后 DMA 完成。
2. 依序关背光、发 display-off、删除 panel/IO、释放 SPI 总线，最后让 GPIO21 成为输入；LCD CS、RESET、背光保持不活动电平。
3. 任一步失败或 DMA 完成不确定立即进入 `fault`，不再次释放或复用不确定资源。SDK 操作不保证硬实时；如果底层调用未返回，状态仍为 `stopping`，不会宣布 `off`。
4. 显示任务、DMA 条带（流水线配置为两个）、信号量和已分配的 PSRAM 双画布固定保留至重启；关屏释放 SPI 的设备/总线，但不声称释放这些内存。重新启动时重新探测两次 ILI9341、配置面板并等待首帧，仍为固定10MHz。
5. `board_display_sd_acquire/release` 由原生SD服务实际使用，只有LCD `off`/`sd-blocked`可获取，持有时拒绝`lcd-on`；卸载/FatFs detach/SDSPI设备和总线清理成功才release。正常SD操作和后续LCD恢复有限实测通过，但电气波形/检测故障或竞态未测。

正常SD/LCD交接有限实测不替代DMA/清理错误注入、电气波形、卡检测异常、LCD独立目视或长期压力。现有上游UBSan对齐诊断仍未消除。

## 后续上板边界

设备写权限已耗尽。任何新固件写入须独立冻结准确hash、字节数和factory实际扇区范围，再获明确批准；之后仍须新同板、8安全字段、最新actual完整0.5.12基线、旧引导预检和新marker，最多一次USB应用写入。失败停止，不自动重试/恢复或写OTA、引导、分区表、NVS、eFuse。上板后的`:help-sd`显示、SD挂载与卡读写是分开验收，不因SD源码/host fake/离线build自动获准。
