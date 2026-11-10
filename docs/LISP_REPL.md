# LispBM REPL＋LCD终端（当前0.2.4 B/VALID，无认证TCP＋UART）

当前0.2.4已准确批准并唯一POST部署B/VALID；正常B只读观察180.146秒，启动版本/IP/端口/nc各一次、无自动help/测试eval。内部单次真实probe保留且成功静默，A0.2.3备用；从今启动观察只读，不自动UART/TCP eval，accept_*默认拒绝，交互另需明确许可。新版本没有主动跑下面历史的12UART/24TCP，不能挪作0.2.4结果。

最新私有参考boot-info-2026-11-10/post-protection/current-0.2.4-expected.bin为重建实际SDK匹配，非新双读，保护核验在最后B180秒前；之后只有只读HTTPS。LCD完整banner已提交，未独立目视；UART初始connecting行可被SDK记录插入，IP/端口/nc原始行完整。

## 历史0.2.3交互验收（当前A备用）

当前0.2.3已通过唯一真实OTA POST部署A/VALID，保留B0.2.2备用。直接nc DEVICE_IPV42323，无cert/token/login，24项真实TCP（含nc）及正常A重启180.049秒/12项UART通过。原owner求值/共享RAM，OTA原TLS/token保留，旧HTTPS/repl移除；不提供Telnet协商/WebSocket/浏览器。命令行及边界见 [WIFI_REPL.md](WIFI_REPL.md)。

本版LISP READY输出和LCD提交已记录，未独立目视本版文字/滚动；用户对旧0.2.1可见标识的确认不能外推新版本。全片/补集/控制/8字段核验在最后A180秒测试前，之后只读HTTPS；最新私有参考是tcp-repl-2026-11-10/post-protection/current-0.2.3-expected.bin，非新双读。所有写授权耗尽，不自动DSP/BT或再刷。

## 用户要求去除REPL cert/token（0.2.3已部署）

新源码改为明文TCP2323/nc，不登录，任何LAN可达者可执行Lisp；OTA原TLS/token保留。旧HTTPS/repl/公开客户端移除，共用remote有界邮箱、原owner与LCD。两模型各88主机测试、后续24项真实TCP通过，已准确批准部署A/VALID，写权限全部耗尽，见 [WIFI_REPL.md](WIFI_REPL.md)。

## 历史0.2.1实机结果（2026-11-10，现已被新A替换）

- 初次只读UART同步失败曾阻断上传；用户重插USB后重新枚举，实际同板/8安全字段/完整4MiB最新基线及BHTTPS状态通过，再执行已经准确批准的唯一POST。不是上传重试。
- POST HTTP200，1104672字节、文件hash `70932bf410f4bbebec4c4f53e9f4020f1f3866f0f1df943f377273ba20eb8c83`，A[10000,11e000)/270扇区；实际启动 `init_stage=ready align_mod8=0 selftest=PASS`，网络GET及实读控制记录确认A/VALID。
- 正常A重启后独立180.052秒：startup_ready=2.121秒、12项真实UART交互全通过（3、定义/lambda/42、错误恢复、Ctrl-U、退格、hello、RAM重置/遗忘/恢复、LISP READY）。最后固定证书HTTPS内容hash/版本/A槽/online/healthy/confirmed匹配。
- LCD仅ILI9341，两个一致9341、固定方向，提交1→851、栈低水位3336字节；心跳0→179。**日志证明SPI提交，不是目视证明；终端文字可读性/滚动仍待用户反馈，不索要新照片。**
- A镜像/FFpadding、3补集（B0.1.7/bootloader/表/NVS/free/旧数据）、8字段、有效控制seq3/seq2 VALID及完整4MiB重建预期通过。所有Flash/security核验在最终A180秒启动/REPL测试前；之后只读HTTPS，不声称再读Flash。
- 最新私有参考 `post-protection/current-0.2.1-expected.bin` 在 `.local/flash/lisp-2026-11-10-aligned/`；重建且实际完整SDK摘要匹配，非新双读。旧0.1.7和失败0.2.0全片预期不能用于当前设备。
- 新旧写入许可已全部消费，串口释放、无后台监控；没有USB写镜像/重传。当前用USB串口输入，蓝牙键盘、音箱、DSP/SD均不在本次实现中。

## 0.2.0首次部署失败历史（2026-11-10）

用户对新hash/范围及标准OTA控制记录另行明确批准后，`0.2.0-lisp-repl-ota` 经一次HTTPS POST/HTTP200写A；1103808字节，SHA256 `1c30b34eb2bac8980f125962755762a4010a7d16ec3bf17f19345407138123e9`，擦写 `[0x10000,0x11e000)`。写前同板/8安全字段/完整4MiB基线及固定证书HTTPS B/VALID/健康通过。

**应用已启动，但 LispBM 初始化失败，REPL未可用。** 健康门控拒绝确认/启动新HTTPS，122379ms记录未确认期限到达并自动重启回退；只读HTTPS确认已回原 **0.1.7 B / VALID / healthy / online**。A控制为seq3/ABORTED，B为seq2/VALID，CRC均有效，B控制扇区未改。实际A镜像/扇区padding及全部补集和8安全字段通过，完整4MiB重建预期亦匹配；不是新双读备份。

**新旧写入许可全部消费，禁止重传、同hash重装或自动USB恢复。** 冻结候选在 `.local/flash/lisp-2026-11-10-repl/`，包含当时预授权文档，不改这些归档；当前结果看authorization/attempt/resume及失败总结。DSP与蓝牙键盘/音箱仍不在本阶段。

### 0.2.0初始化失败的线索（不声称唯一根因已证实）

- Xtensa编译静态断言确认：169120字节运行时结构的自然对齐为4字节；现有 `heap_caps_calloc` 没有明确要求8字节。
- 上游 `lbm_heap_init` 明确拒绝非8字节对齐的heap，这是已发现的接口契约缺陷；64位主机calloc测试未覆盖这个目标ABI风险。
- 当前失败日志只报告整体初始化失败，没有实际指针/具体失败阶段，**不能将对齐风险直接称为本次唯一已证实根因**。下一候选应明确8字节分配、增加分阶段诊断并补目标ABI/初始化检查，再重新验收。
- 当时回退B后的参考 `post-protection/current-after-failed-lisp-expected.bin` 已完成实际SDK匹配；该历史预期现已被当前0.2.1参考替代，不能用于新全片核验。

## 0.2.1修复实现与离线验证（后续实机结果见最上节）

- PSRAM存储改用 `heap_caps_aligned_calloc(8, ...)`；结构明确 `_Alignas(8)`，入口按8字节检查，Xtensa编译静态断言验证，不再依赖32位结构的自然4字节对齐。
- 初始化按固定上游顺序拆为 memory/symbols/heap/print/extensions/evaluator/image/print-extension 阶段，失败显示具体阶段和 `align_mod8`，不打印指针/凭据。上游源码未修改。
- 在UART接受输入和OTA健康放行之前，真实执行 `(+ 1 2)` 并检查结果3；失败停止REPL且不确认新镜像。RAM重置成功后保留本次启动已通过的probe标志，初始化失败仍使ready=false。
- 默认64位模型74项C ASan/UBSan通过；额外32位LispBM值模型同样74项通过。后者使用已有Rosetta执行x86_64/64位指针进程、低地址mmap，无MAP_FIXED；**不是原生32位Xtensa运行或PSRAM硬件证据**。
- Xtensa编译-only回归要求存储对齐≥8；旧源码RED、新源码GREEN。八个初始化阶段分别做主机故障注入、验证ready保持false及RAM重初始化恢复。
- 对齐接口已修正且0.2.1实机初始化/交互通过，但旧失败日志没有指针/阶段，仍不声称已证明旧0.2.0唯一根因。0.2.0冻结档案/失败记录不改；新候选必须重新准确批准，不接受未来未知hash的无限刷写授权。

## 第一阶段范围

- LispBM 真实解释器：算术、列表、定义、lambda、求值错误与一个原生 `print` 扩展。
- UART0 / 115200 输入；不改原 UART 引脚与波特率。现有 SDK 日志也使用 UART，可能穿插 REPL 输出；LCD 只显示 REPL 内容。
- 固定 ILI9341 320×240、8×12 字符单元、40列×20行：19行输出滚动＋末行输入窗口。ASCII 32–126，保留大小写；不支持中文、ANSI/VT100 全协议、历史浏览或编辑器。
- Enter 求值；括号/数组/字符串未闭合时继续下一行。扫描器只判断续行，真正语法检查由 LispBM 完成。
- BS/DEL 删除当前行末字符；Ctrl-U 清除当前行；Ctrl-C 取消输入/求值并清除**全部 RAM 定义**。
- `:help` 帮助、`:clear` 清 LCD 输出但保留定义、`:reset` 清全部 RAM 定义，不重启 ESP32。
- 每次只接受一个提交；求值期间除 Ctrl-C 外不接收新源码。不要粘贴多条独立提交期待自动排队。
- 不写 NVS、分区、文件系统，不加载或保存工作空间，不注册 GPIO、网络、DSP、音频或任意内存访问扩展。

示例（当前UART或Wi-Fi客户端）：

```lisp
(+ 1 2)
; => 3
(define twice (lambda (x) (* x 2)))
(twice 21)
; => 42
(print "hello")
```

## 所有权与资源边界

```text
UART RX → REPL owner → LispBM 单线程分步求值
                 ↓ 输出/输入文本模型的值拷贝
            现有 LCD owner → RGB565 条带 → SPI/DMA
```

- 只有 REPL 任务调用 LispBM；使用 `LBM_SINGLE_THREADED` 和无分配的单所有者 mutex 适配。不适用于多个线程直接调用解释器。
- 解释器固定存储用明确8字节对齐的 `MALLOC_CAP_SPIRAM` 分配，约165KiB；8192 cons cells、64KiB通用内存、32KiB RAM image 及 bitmap/reader。PSRAM 不可用时停止 REPL，不挤占内部 DMA/任务堆。任务栈6144字节放内部 RAM。
- 上游 image 是常量符号所需的数据结构，不是设备固件镜像；本项目的唯一 image writer 只写这个有界 RAM 数组，不接入 Flash API。
- 输入累计最多1023字节；超长后拒绝求值，必须 Ctrl-U/Ctrl-C 清除。UART FIFO/环形缓冲/帧错误也取消会话，绝不执行悄悄截断的代码。
- 每提交前执行 GC；输入复制到解释器独占 reader 存储。没有例程中“延时100ms后盲目复用源码”的竞争。
- 每批64个求值步骤，任务每轮至少让出一个 tick；提交5秒超时后清 RAM 定义。**这是协作式保护，不是每个原生操作都保证5秒内可抢占，也不是安全沙箱或硬实时保证。**
- 每提交 print 输出限8KiB；超过时提示截断，最终求值结果独立保留输出额度。单个结果格式化限512字节，超限显示占位符。
- 第一阶段不保留跨提交后台 Lisp context；根 context 完成后若仍有 detached contexts，则报告错误并清 RAM 定义。后台音序器/事件调度留待第二阶段设计。
- LCD 仍由原任务独占，保持两次一致9341才初始化一次、固定方向和10MHz SPI；未知 ID 停止，不新增驱动/引脚/电压尝试。终端模式目标提交周期200ms；DMA超时隔离资源的逻辑不变。提交成功不是目视验收。
- REPL关闭时仍是原诊断画面。OTA启用时，健康确认还要求 REPL 已初始化且 owner 有近期进度；保留已有 PSRAM/心跳/LCD/Wi-Fi/HTTPS及回退条件。

## 安装锁定依赖

上游：[svenssonjoel/lispBM](https://github.com/svenssonjoel/lispBM)，版本0.41.0，commit：

```text
cbb30151bcb167c1035c16907b45ba1cb8a40e46
```

源码只留 Git 忽略的 `.local/lispbm/`，不自动下载，不修改上游。
已有目录不要重复安装；新环境可用：

```sh
umask 077
git clone https://github.com/svenssonjoel/lispBM.git .local/lispbm
git -C .local/lispbm checkout --detach cbb30151bcb167c1035c16907b45ba1cb8a40e46
```

`cmake/lispbm.cmake` 检查准确 commit 和 tracked 文件未修改；可通过 `-DLISPBM_SOURCE_DIR=/path/to/checkout` 指定位置。
默认关闭构建、默认主机测试不需要 LispBM。上游代码声明 **GPLv3或更新版本**；项目许可证未改变，分发组合固件前须另行核查兼容性和义务。当前私有固件含既有凭据/密钥，不能公开分享。

## 主机测试

```sh
cmake -S tests -B .local/host-build-lisp -G "Unix Makefiles" -DENABLE_LISPBM_TESTS=ON
cmake --build .local/host-build-lisp --parallel 4
ctest --test-dir .local/host-build-lisp --output-on-failure
```

当前74项 C ASan/UBSan：原45项＋终端6项＋输入3项＋真实解释器11项（新增启动probe）＋初始化阶段故障8项＋端到端1项。旧0.2.0的65项结果仅为历史。
覆盖大小写/标点、条带逐行一致、边界/滚动/长行窗口、CRLF/续行/转义/字符字面量/溢出、算术/定义/错误恢复/print、30次RAM重置、1500次连续求值与GC、超时、detached context及输出上限。
端到端用 UART 字节前端→真实 LispBM→LCD文本→RGB565条带，**不模拟真实UART/SPI/FreeRTOS，也不算硬件验收**。

额外的32位值模型（本机已有Rosetta，不安装或改变主机安全/网络配置）：

```sh
cmake -S tests -B .local/host-build-lisp-word32-x86 -G "Unix Makefiles" \
  -DENABLE_LISPBM_TESTS=ON -DLISPBM_HOST_WORD32=ON -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build .local/host-build-lisp-word32-x86 --parallel 4
ctest --test-dir .local/host-build-lisp-word32-x86 --output-on-failure
```

macOS arm64不允许启动缩小PAGEZERO的ARM64可执行文件；配置现在提前拒绝该组合。这里只在测试进程使用x86_64低地址映射，不覆盖现有映射。该刻意的value32/pointer64模型会报告上游指针转换警告（仍显示，不提升为错误），不是目标ABI等价替代。Linux分支尚未本机验证。

## 离线构建

使用现有固定 IDF 环境，新独立目录，不覆盖部署/冻结目录：

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-lisp-aligned-dev \
  -DSDKCONFIG="$PWD/.local/sdkconfig.lisp-aligned-dev" \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;tests/sdkconfig.psram.defaults;tests/sdkconfig.lcd.defaults;tests/sdkconfig.lisp-repl.defaults" \
  -DCCACHE_ENABLE=0 -DPROJECT_VER=0.2.1-lisp-dev build
```

`CONFIG_LISP_REPL_ENABLE` 默认关闭，依赖已确认LCD及PSRAM。
该离线 profile 用当前分区CSV仅做容量检查，Wi-Fi/HTTPS/OTA服务关闭，**不能作为可持续OTA更新镜像上传**。
另有 `.local/build-lisp-ota-check/` 仅编译检查现有STA/HTTPS/回退与REPL组合；它含原私有输入，只留本地，不是已批准/已冻结候选。

自有代码严格编译和格式检查；上游单线程配置在Xtensa GCC产生4条未使用变量/`longjmp` 警告，未修改或屏蔽上游。主机cJSON仍有既有6条Apple SDK deprecated警告。这不是第三方安全审计。

**不要执行构建工具打印的 flash 命令。任何上板需新的准确hash、槽和擦写范围批准。** 见 [安全边界](SAFETY.md) 与 [OTA](OTA.md)。

## 已部署冻结候选与范围

`0.2.1-lisp-repl-ota` 已在新的 `.local/build-lisp-ota-aligned/` 构建，并冻结在 `.local/flash/lisp-2026-11-10-aligned/`：1104672字节，SHA256 `70932bf410f4bbebec4c4f53e9f4020f1f3866f0f1df943f377273ba20eb8c83`，A镜像 `[0x10000,0x11db20)`、擦写 `[0x10000,0x11e000)`/270扇区；另须仅SDK控制 `[0xc000,0xe000)` 生命周期许可。初次预检阻断后用户重插，新完整预检通过才执行唯一POST，授权已消费。当前执行事实看authorization/attempt/resume/acceptance-summary；冻结candidate/source的旧PENDING文档不改写，也不作为新许可。

保留B/旧数据/bootloader/表；这份准确批准的一次POST已消费，不能重传/恢复。旧A0.2.0 ABORTED和新全片恢复参考见上文，不能再拿更旧0.1.7全片预期做预检。冻结源文档是冻结当时的描述，不因后续文档更新改写归档。四个新配置构建、74项默认/74项额外值模型、15 Python/4 uploader mock及格式/静态分析通过，不代替设备验收。

## 后续顺序

1. 当前UART交互、启动probe和OTA状态通过，等待用户确认LCD文字/滚动。新候选需新的准确hash/范围批准；不能重用本次已消费的许可。蓝牙键盘/长时GC/更多压力作为独立后续需求再设计和验收。
2. 再设计 Lisp 控制接口与原生音频/DSP任务，最后接入 A2DP 音箱；不把解释器/GC放在逐采样实时路径里。
