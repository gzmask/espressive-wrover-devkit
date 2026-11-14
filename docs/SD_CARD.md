# SD 卡与 Lisp API（0.5.12已部署，三demo有限实卡闭环通过）

## 最新demo验收：SD加载→取卡→LCD恢复→短运行/q退出通过

用户已允许既有demo的SD加载及后续有界运行/退出。新同卡身份/容量/FAT32/MBR/UUID检查后，按原源码独占创建DEMO.LSP658、TINY2D.LSP1096、TINYGL.LSP2438字节，全部精确读回/force并安全弹出，无覆盖旧APP.LSP或其他数据、format/repair/固件写。一个TCP连接确认LCD off/保持3.5秒、SD unmounted、会话变量42，端口释放。用户插卡后一个TCP会话唯一mount、各份一次app-load-sd全loaded；descriptor各3、动画phase0/pausednil、TinyGL表32/顶点8/边12。诊断实传658/1096/2438字节分别匹配、fat/close/error0/quarantine0、worker trace栈余量3860字节（非独立峰值或全容量保证）；唯一unmount后全部定义及会话42仍在。用户取卡后一个新TCP会话唯一LCD on并保持3.5秒，demo/tiny2d/tinygl各一次app-run、5秒后仍持有应用、各一次:key q正常退出并等待终端恢复。每个定义和会话42保留，最终仅清理会话变量；三demo保留RAM、全部已退出。tiny2d/tinygl退出phase21/4为模32观察，不能比较CPU/FPS或展开回绕。最终SD unmounted/LCD on、卡在板外、TCP释放；无新SD访问/源重载/UART/复位/Flash/security/重试重连。逻辑短运行/恢复不等于LCD独立目视/FPS或长期稳定；32KiB及目标heap峰值/取消故障仍未测。完整证据demo-acceptance1/{sd-demo-loads1/summary.json,lcd-on-demo-runs1/summary.json,execution-final-resume.json,execution-CONTINUE-final.md}。

## 前次31字节闭环：卡已取出/LCD on

准确许可后唯一factory USB写入0.5.12，完整4MiB重建预期及8字段保护通过；一次180.143秒只读启动通过，worker正常启动。此次启动未插卡/发送REPL或SD命令。随后31字节实卡读取/关闭/拷贝/求值通过；32KiB运行时堆/PSRAM、取消/迟到worker仍未目标验证。用户已授权31字节`APP.LSP`（仅`(define app-load-sd-check 512)`与LF），同一卡fresh身份/FAT32/MBR/UUID核验后独占创建、force、精确读回、安全弹出；没有覆盖同名文件/格式化/修复。随后一个TCP连接将LCD on→stopping→off并保持3.5秒、SD unmounted、RAM测试定义42仍在，端口释放。用户插回板卡后，一个新TCP会话唯一mount/app-load-sd/unmount成功；状态loaded、变量从-1变512，卸载后512和会话42保留。源诊断phase=close/failed=idle/error=0/fat=0/close=0/bytes=31/32769/quarantine=0；三次操作trace报告worker stack_min=3860字节（任务6144），不是每操作独立峰值或32KiB安全保证。用户取卡后独立TCP唯一lcd-on，off→starting→on并保持3.5秒；SD unmounted，512及会话42保留后仅清理本任务定义，卡在板外、TCP释放；无普通sd-read/sd-write、format/app-run/UART/主动复位/Flash/security/重试/重连。证据位于`.local/flash/sd-app-load-0.5.12-exec1/loader-acceptance1/`的host-file-preparation1、sd-load-device1、lcd-restore-after-card-removal摘要及execution-final-resume.json。

`0.5.12-sd-app-load-usb`：SHA256`febb3d42d45888035c5d2cb625d6513cb9946e36334e261f23fadb70fa3525ea`，1,257,264字节，镜像`[0x10000,0x142f30)`，擦写`[0x10000,0x143000)`／307扇区。用户对准确hash/range consent后唯一USB应用写成功并完成保护和只读启动。启用SD，主`:help`列`app-load-sd`；普通`sd-read`仍240字节。冻结目录`.local/flash/sd-app-load-0.5.12-freeze1/`中的PENDING只代表冻结时状态，不是当前授权状态。



在SD启用的开发配置中，先关闭LCD、插卡、挂载到`mounted`，然后提交`(app-load-sd "APP.LSP")`；`t`只表示请求已排队，通过`:load-status`等待`loaded`，或`:load-cancel`取消。加载完成后**卸载、取卡、恢复LCD**，再显式`(app-run demo)`。源文件在卡根目录，文件名遵守现有安全规则及实际FAT配置；当前配置关闭长文件名，建议8.3名称如`APP.LSP`。

SD worker以512字节块读取最多32768字节ASCII到独立32769字节PSRAM缓冲；空、超限、非法字节、读取或关闭失败均不执行源码。原Lisp owner复制完整源码到既有32KiB源码区后才释放SD缓冲，随后求值；URL与SD共用一个加载状态机，不并行加载。取消/超时不会复用仍被worker持有的缓冲；读阶段取消保留已有定义，求值阶段取消清全部RAM定义。普通求值错误不回滚已执行定义，不是沙箱。失败可通过`sd-last-error`/`sd-diagnostics`和`:load-status`排查；关闭失败保留故障隔离，不自动重试。

源码在SD worker启动时申请32769字节PSRAM，分配失败只使SD app loader不可用，不使用内部RAM回退；另有URL下载缓冲及Lisp固定源码区。现有`sd-read`仍限240字节，未改变。主机LispBM+mock后端测试32KiB拷贝/取消/迟到结果，真实FatFs堆内存盘测试32KiB/超限及close错误模型；不是板上FreeRTOS/SPI证据。0.5.12的31字节实卡加载已通过，表明该路径的预留PSRAM源码缓冲可用；worker观测stack_min=3860字节。未测32KiB、内部heap/PSRAM峰值/最大连续块、取消或故障；不能以一次成功外推全容量/长期稳定。

## 主`:help`中的SD命令：已随0.5.12部署

0.5.12在SD启用时主`:help`直接列出`(sd-mount) (sd-read ...)`、`(sd-write ...) (sd-result)`并指向`:help-sd`；帮助保持LCD19行限制。`app-load-sd`也列在`:help-sd`完整说明中。开发构建和两主机组合测试通过；板上主`:help`输出未专门交互检查；实卡`app-load-sd`已完成31字节有限验收，详见本节最上部。

## 历史设备：0.5.11部署/实卡读写通过，随后被0.5.12替换

用户准确批准后，新同板/8安全字段/完整0.5.10实际基线/旧引导预检通过，唯一USB应用写入69.5秒，准确app与FFpadding、全部补集及完整4MiB预期匹配；8安全字段不变。参考是实际SDK匹配0.5.10参考＋新app重建，非新双读。随后卡保持主机读卡器，一次180.174秒正常RTS只读启动通过：0.5.11/2161ms ready/probe PASS/PSRAM六模式，SD worker打印`stack=6144 workspace=4436 FIL=4136`，ILI9341帧1→1351、心跳0→177、ONLINE8511ms/TCP8581ms、34条healthy，联网前仅一次unhealthy。无UART/TCP表达式、SD操作、LCD目视或启动后Flash/security读取，端口释放。

随后用户明确插卡，在LCD off/SD unmounted后一个TCP会话完成：挂载2次、仅对新`S511D001.TXT`写入一次、两次读取均精确匹配57字节ASCII payload、卸载2次且最终unmounted；每个成功操作后last-error=0。无格式化、重试、重连、UART/复位/Flash/security/NVS/eFuse。用户取卡后独立TCP恢复LCD on并清除自有RAM定义。

这证明此短FAT32卡和软件卸载重挂路径可用，支持栈改动没有立刻重现旧超时；**不证明**旧超时唯一根因、断电持久性、电脑独立读回、不同卡、文件系统长期完整性、错误注入、worker实际low-water或LCD目视。当前卡不在板上、LCD on/SD unmounted。新SD任务需明确请求并先关LCD再插卡；不得自动格式化或补发旧`S7D9C21A.TXT`请求。最新记录`.local/flash/sd-stack-0.5.11-exec1/sd-retest/`。

## 历史候选：0.5.11准确批准并部署

`0.5.11-sd-stack-diagnostics-usb`：1,254,992字节、SHA256 `bfbbb25db9a2d06832663941d5277e083e260874ed0e065b97b0903306ab1338`；镜像`[0x10000,0x142650)`，实际擦写`[0x10000,0x143000)`／307扇区，比部署0.5.10多一个末端扇区。新独立冻结`.local/flash/sd-stack-0.5.11-freeze2/`含205源码快照、应用/ELF/config、完整4MiB离线模型、候选前后审计和32项SHA256摘要。仅申请一次factory应用USB；表/引导/NVS/eFuse和范围外不写，单槽无回退。此前泛称继续仅允许离线冻结，**不等于该准确hash/range批准**。

候选基于base commit`fcf5c9e`未提交工作树；新candidate2构建前后源码一致，2×210 CTest、28Python/4历史mock、堆内存FatFs、三配置构建、格式/分析和三私有负例通过。初次freeze1在建目录前仅因esptool紧凑image_info缺少版本文本而阻断；版本已由镜像descriptor校验，改用新snapshot2/新build冻结，非设备或固件重试。实卡效果仍NOT TESTED。批准后仍须新同板8字段/当前actual完整0.5.10基线/旧引导、新marker一次USB、完整保护和180秒只读启动；卡须板外。

## 已部署：栈预算修复和阶段诊断（实际worker操作水位仍未测）

`FIL`（含SDK配置决定的4KiB缓存）、48字节路径、读取缓冲及worker请求改为单worker独占的静态内部DRAM对象；没有全局共享文件句柄供REPL/其他任务调用。任务栈仍6144字节，新增静态存储约5036字节（文件workspace4436、请求560、诊断40），不声称总RAM更少。Xtensa编译报告：worker栈帧48、read/write包装32/48、文件核心各64字节；旧worker592、read/write4928/4688。组件启用`-Wframe-larger-than=768`和`-fstack-usage`阻止大自动对象回归；这只限制单个函数栈帧，不能证明完整SDK调用链或硬件峰值安全。

新增只读`(sd-diagnostics)`与UART阶段日志，记录当前phase、首个失败phase/ESP错误、FatFs结果、独立close结果、传输计数及worker生命周期栈余量采样。诊断快照只在短锁内复制；不跨FatFs/SPI或日志持锁，不记录文件名/内容，不持久化到Flash，复位即清。此API **板上0.5.10尚不存在**。读结果改为复制完毕后仅发布一次read-ready，移除原read_file和finish_io两次发布的并发窗口。

写/sync返回FatFs错误或close失败时隔离仍打开的文件对象，进入fault并保留SD租约/总线；不调用可能再次同步脏缓存的close重试、不复用句柄、不自动恢复。fault不因sd-result而清除，禁止卸载/再读写/LCD-on。FR_OK但短写会关闭文件并报告operation-error，可能留下部分内容，不保证事务回滚。读失败仍只对已打开的只读文件尝试一次close；close失败同样fault。发生隔离后不要拔卡、开屏或自动复位，须停下制定受控断电/取卡方案，不能把fault当作unmounted。

两主机模型各210项CTest、28Python、4历史mock、格式和文件核心/诊断Clang分析通过。新增真实IDF FatFs/FAT32 **仅堆内存盘**20轮写/卸载重挂/读回、截断/空文件/缺文件/同步错误测试，以及真实4KiB FIL布局下的文件核心错误注入；同步/写失败不close、不复用、首错/close分开保留与边界覆盖。三个私有破坏版本（自动FIL回栈、sync错误后close、覆盖首错）均被编译门禁或断言拒绝。主机mutex为单线程替身，不是FreeRTOS/SDSPI/设备证据，上游既有UBSan对齐诊断仍在。

`.local/build-sd-stack-{full,uart,disabled}`三个独立开发构建通过：SD+TCP/URL+DMA/perf、SD UART/no-WiFi、SD关闭full；full开发镜像1254992字节/余54%，非冻结或批准镜像。SD关闭ELF不含worker/workspace/新API。此轮无设备/主机SD卡访问、UART/TCP、复位或固件写入，旧冻结/实际证据不改，未commit/push。将来部署仍须新准确hash/实际范围批准及最新0.5.10基线保护；不能据离线修复宣称原写入超时根因已证实或实卡已修复。

## 最新检查：主机未找到测试文件，LCD已恢复

同一卡回到电脑后，身份/容量/FAT32/MBR及卷UUID核对通过，指定测试文件`S7D9C21A.TXT`不存在，64字节写入未得到证实。一次只读文件系统验证180秒超时，不能声明一致性通过，也不据此认定损坏；无修复/补写/格式化。

卡仍在电脑，板上LCD经一次TCP恢复到on并保持3.5秒，SD unmounted；自有RAM测试定义已清理，无UART/复位/固件操作。不要在LCD on时插卡。

离线发现候选栈预算风险：6KiB SD worker中worker自身592字节、write_file栈帧4688/read_file4928；FIL含4KiB私有缓存，仅余864/624字节供SDK嵌套调用。需要离线消除大自动对象并完善失败阶段诊断；无原崩溃日志，尚不能确认实际栈溢出或唯一根因。未因此修改/重刷固件。证据`.local/sd-host-readback-fat32/analysis.md`。

## 历史FAT32实卡尝试：写入回复超时（后来主机检查文件不存在）

用户将已格式化的FAT32卡插回后，一次`sd-mount`成功、状态mounted且last-error=0。随后只发送一次测试文件`S7D9C21A.TXT`的`sd-write`，等待回复超时；**未收到ACK，不能判成功或失败，更不能重发写入**。没有读回或显式卸载命令。

后续独立状态对账连接仅查询`sd-status`，得到unmounted，随后关闭；未补写/重试/复位/UART/格式化/固件操作。最新软件状态允许取卡，但未证明文件完整或clean flush，具体故障/重启原因未观测。请将卡移至电脑读卡器，只读检查指定文件和文件系统；不自动修复或格式化。证据`.local/flash/sd-card-0.5.10-replug1/card-acceptance/fat32-result.json`。

## 历史介质准备：用户授权的FAT32格式化已完成

用户确认卡为exFAT并明确允许格式化，随后指明电脑上的64GB Untitled。Mac内置读卡器中可移除SD卡实际63.6GB，经设备/容量/CID及卷UUID核对后唯一格式化为单分区MBR＋FAT32，卷名`WROVERSD`、32KiB簇，写后同卡/分区/文件系统验证并安全弹出。没有ESP32/UART/TCP/固件或其他磁盘写操作；这是普通快速格式化，不是安全数据销毁。卡可插回LCD仍关闭的开发板；确认插好后进行新的FAT32挂载读写验收，不再格式化。当前FAT32实板读写仍未测试。证据`.local/sd-format-64gb-untitled/result.json`。

以下首次挂载失败属于格式化之前的exFAT卡，不挪作新FAT32卡的结果。

## 历史exFAT实卡结果：挂载失败，没有文件写入

用户插卡并明确允许测试文件后，一次`sd-mount`返回接受`t`，随后状态`mounting→unmounted`，`sd-last-error`为`-1i32`（ESP_FAIL）。会话已停止，无重试、复位或格式化；`sd-read`/`sd-write`均未发送，计划文件`S7D9C21A.TXT`未创建。最后SD软件状态unmounted、LCD最后观测sd-blocked；可取卡，确认取出后才能恢复LCD。

此前host脚本将合法sd-blocked误作异常，只读两状态便停止；修正为源码支持的off/sd-blocked后才发送上述唯一挂载请求，未重复任何存储操作。当前接口未保留失败阶段/FRESULT，不能仅凭ESP_FAIL断言卡格式、兼容性或SPI根因。用户随后报告卡为exFAT，SDK`FF_FS_EXFAT=0`确认此文件系统不受支持；这与挂载失败一致，但未证实失败的精确阶段或排除其他卡/SPI问题。后续应换已备份的FAT32测试卡；不自动格式化或修复当前卡。证据`.local/flash/sd-card-0.5.10-replug1/card-acceptance/result.json`。

**原部署只读启动范围**：0.5.10已唯一USB应用写入，写后实际完整4MiB SDK摘要与重建expected匹配、8个安全字段不变，用户明确继续后一次180.142秒只读启动factory/healthy通过；写入权限耗尽。没有SD插拔、挂载、读写或格式化，实卡行为未验收。真实卡交互须用户明确请求，持久写入须另行许可。详见`.local/flash/sd-card-0.5.10-replug1/readonly-boot-continuation/acceptance-summary.json`。

## 历史：实卡验收准备（随后已尝试挂载，失败见首节）

用户要求继续完成验收后，0.5.10经一个TCP连接实际确认SD `unmounted`，一次关屏后`stopping→off`，保持3.5秒REPL和RAM定义正常；TCP已释放。当前LCD `off`，等待用户插入已备份的空白FAT/FAT32测试卡及允许指定测试文件持久写入。尚未挂载或读写卡，不复位/刷写/格式化。全部测试完成并成功卸载后，必须由用户取卡才能恢复LCD。证据另存`.local/flash/sd-card-0.5.10-replug1/card-acceptance/prepare/summary.json`，不是原部署启动阶段交互。

当前`sd-read`把多种FatFs失败合并为一般错误，所以“读取失败”不证明文件不存在；本API没有独占创建模式。测试应使用空白已备份卡与约定的8.3测试文件名，不承诺不会截断碰巧同名文件。

## 历史阻断（后来继续启动并通过）

**最新状态**：用户明确继续后，完整4 MiB实际SDK摘要与重建expected匹配、8个安全字段不变；随后180秒只读启动采集在串口构造前因执行目录0755被安全断言阻断，未实际复位或采集。目录已修为0700，旧marker保留，停止等待用户明确继续。无SD操作、无更多固件写权限；参考非新双读。详情见`.local/flash/sd-card-0.5.10-replug1/execution-boot-blocked.json`。

源码已实现为opt-in SDSPI＋原生FatFs API。`0.5.10-sd-card-usb`已获准确批准并写入一次；初次写后验证因helper路径错误在串口访问前exit2，随后用户明确继续并通过实际完整4MiB SDK摘要及8安全字段核验。用户确认卡已取出；无挂载/读/写/格式化。首次启动采集因主机权限断言阻断，用户再次明确继续后180秒只读启动通过。

## API（0.5.12当前部署）

启用 `CONFIG_BOARD_SDCARD_ENABLE=y` 后，REPL提供异步API：

```lisp
(lcd-off)                         ; 若LCD开着，先请求关闭
(lcd-status)                      ; 必须等 off
(sd-mount)                        ; 请求挂载，返回 t 只表示接受请求
(sd-status)                       ; 轮询 mounting -> mounted
(sd-write "notes.txt" "hello\n") ; 创建或截断并写入纯文本，异步
(sd-status)                       ; 轮询 writing -> write-done
(sd-result)                       ; 写成功返回 t；错误返回 error
(sd-read "notes.txt")             ; 异步读取
(sd-status)                       ; 轮询 reading -> read-ready
(sd-result)                       ; 返回文件文本
(sd-last-error)                   ; 最近ESP错误码，0表示没有记录错误
(sd-diagnostics)                  ; 新源码：阶段/FatFs/栈快照，不触碰卡
(sd-unmount)                      ; 异步卸载；等待 status=unmounted
(lcd-on)                          ; 先取卡/确认已拔出，再请求开屏
```

状态包括 `unmounted`、`mounting`、`mounted`、`reading`、`writing`、`read-ready`、`write-done`、`operation-error`、`unmounting`、`fault`。任务异步处理，`t` 只是请求已入队；完成以状态/`sd-result`为准。只有一个操作队列项；操作未完成/结果未取走时，新的读写和卸载请求被拒绝。读结果最多240字节且必须为有效ASCII文本；写入最多512字节。仅普通ASCII文本，允许TAB/CR/LF，不支持二进制。文件名为1–32个安全ASCII字符，仅字母/数字/下划线/连字符和最多一个内部句点；不支持目录、绝对路径或路径分隔符。

`sd-write`会**创建或截断同名文件并持久写入SD**；卡拔出、断电或写错误可能留下部分内容。本版本没有删除、重命名、格式化、目录、二进制/随机访问或自动重试API。挂载错误**绝不自动格式化卡**；不支持或损坏的文件系统只报告失败。需先备份卡内重要数据。文件系统与卡内容是持久的，不是RAM-only Lisp环境；不要存放敏感数据。

### 新诊断字段

- `phase`为当前/最后执行阶段；`failed`保留首个失败阶段，`idle`表示尚无失败。后续cleanup不覆盖首错。
- `error`为首错ESP码；`fat`为首个错误对应FatFs返回码（无错误时为最近返回值）；`close`独立记录关闭结果。`-1`表示未取得该FatFs返回值，不代表FR_DISK_ERR。
- `stack_min`是trace点采到的SD worker自启动以来最小剩余**字节**，包括SDK/此前日志影响；未采样为unknown，不是每操作的独立峰值、更不是硬实时/不会溢出的保证。
- `bytes`为已报告传输量/请求量（读使用241字节探测过长文件），不等于断电后落盘确认；`seq`在新接受请求时递增，复位清零；`quarantine=1`表示保留不确定资源。
- 取结果不会清诊断，新接受请求会重置该次记录。若操作卡住但owner仍运行，可查询阶段；若芯片已崩溃/复位，RAM诊断不能代替事前UART采集。

## 资源互斥和WROVER-KIT v4.1引脚

此配置选择板上卡槽的 **SDSPI模式**，沿用官方ESP-IDF ESP32示例引脚：CLK GPIO14、MOSI/CMD GPIO15、MISO/DATA0 GPIO2、CS/DATA3 GPIO13。GPIO21是LCD D/C与卡检测共线；项目LCD生命周期租约必须已成功把LCD置为 `off`，然后才允许SD worker占用SPI host并配置GPIO21卡检测。卡槽检测在SDK中按高电平表示“卡缺失”；板级GPIO21低电平表示检测开关闭合，必须由实际硬件确认，不能将软件模型当成实测。

**操作顺序：**LCD开着时先 `(lcd-off)` 并确认 `(lcd-status)` 为 `off`，再插卡；随后 `(sd-mount)`，轮询到 `mounted` 才读写。完成后 `(sd-unmount)`，轮询到 `unmounted`，再取卡，然后 `(lcd-on)`。**LCD显示 `on`、`starting` 时绝不热插拔SD卡。** 启动/复位时固件仍会尝试初始化LCD；因此启动前不能让卡留在槽内，除非实机检测已证明会可靠进入 `sd-blocked`。

SD SPI也占用JTAG GPIO13/14/15，挂载期间不能同时使用这些线做JTAG；GPIO2是ESP32启动绑带线，需保持卡及卡槽在启动期不破坏启动条件。SPI仍是GPIO Matrix/SDSPI，并不提供与SDMMC 4-bit相同的带宽。SD读写、卡识别和供电/信号完整性还没有目标实测。

## 历史0.5.10离线构建和主机验证证据

`CONFIG_BOARD_SDCARD_ENABLE` 默认关闭；专用 `.local/sdkconfig.sd-card-dev` 显式开启。固定ESP-IDF v5.5.6下SD-enabled full（保留LCD DMA流水线和perf诊断）独立构建通过，application bin `0x131ca0`字节；SD-disabled full为`0x125060`；SD-enabled UART/no-WiFi配置也通过。均为私有开发镜像，不是冻结/批准候选，未刷写。

SD服务直接用FatFs逻辑盘API，不注册POSIX VFS。最初POSIX VFS挂载原型因额外时间/VFS IRAM使用触发约740字节IRAM超限，纯离线诊断后改为直接FatFs mount/open/read/write/sync/close；最终启用和禁用配置链接通过。此过程不是设备错误或固件重试，也不声称SD硬件路径已通过。

两种LispBM主机模型各191项CTest、28Python/4历史mock通过；新增生产Lisp扩展集成使用service fake，覆盖LCD-off挂载门控、异步挂载/读写状态/结果/错误、文件名/文本边界和RAM定义保留。fake不是RTOS worker/SPI/FatFs实机证据；C格式/纯状态Clang分析通过，上游UBSan对齐诊断保留。

## 实现边界与测试

- 固件不启动时自动挂载、不创建/格式化文件、不扫描/列举卡内容。`sd-mount`要求LCD生命周期租约获取成功；租约持有期间拒绝 `lcd-on`。
- FATFS使用原生逻辑盘API（无VFS挂载、无文件名外部路径），配置 `format_if_mount_failed=false`、最大2个打开文件；单worker串行处理读写，Lisp提交有界副本，不在REPL owner、LCD回调或SPI任务做卡I/O。
- 文件操作串行；单个读结果240字节、写入512字节文本，有界路径；不开放任意OS命令或POSIX VFS路径。文件名限制仅防路径穿越/路径注入，不是Lisp安全沙箱。
- 故障状态保守隔离；卸载/总线清理失败时保留SD租约，不宣称已释放引脚，不自动重试或释放不确定资源。
- 主机逻辑测试覆盖文件名/文本边界、lcd-off和SD租约状态策略、异步操作状态及LispBM扩展；主机fake不测试FreeRTOS/SPI/FATFS/card-detect电气行为。GPIO、SPI/DMA、卡识别/兼容性、文件系统和真实卡读写均为 **NOT TESTED**。

以下为旧候选执行历史，0.5.10后来已部署；新栈修复源码仍未部署。`0.5.10-sd-card-usb`独立候选已离线冻结且用户准确批准，SD API明确启用，出厂表保持不变。批准范围如下；实际执行在写前全片verify连接失败后停止。

- SHA256 `7ef04268894d093064c8edf5ab1fd88c31921557a60e99256cd1514fc4b98964`，1,252,512字节。
- 镜像 `[0x10000,0x141ca0)`；实际擦写 `[0x10000,0x142000)`，306个4KiB扇区，比0.5.9多13个末端扇区。
- 仅申请一次factory USB应用写入；表/引导/NVS/reserved/free/旧数据和全部范围外保护。单槽无板上固件回退。保护通过后180秒启动检查只读，SD卡交互不包含在固件批准范围内。
- 基于未提交工作树（base commit `fcf5c9e`）独立构建，未纳入SD代码的0.5.9镜像不受影响。候选本地目录 `.local/flash/sd-card-0.5.10-freeze2/`。

用户批准后执行新同板/8字段/actual完整0.5.9基线/旧引导预检；重插后唯一`write_flash`成功并报告data hash verified。初次写后verify因helper路径错误在接触设备前exit2；随后用户明确继续，通过实际完整Flash SDK摘要与8安全字段只读核验。无第二次写、重试或SD卡I/O；180秒启动采集另因目录权限在打开串口前阻断。固件写权限已消费；等待用户明确指示后只做独立只读启动，不重写/恢复。

SD-enabled/perf完整开发构建通过、SD关闭的0.5.9配置复编通过；这不能证明实卡检测、挂载或文件IO。候选已写入一次，full Flash/security保护及180秒只读启动通过。**不得自动再写或恢复；实卡交互须用户另请。**
