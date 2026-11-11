# LispBM 前台应用层（当前0.5.7 factory，物理LCD FPS未测）

## 已部署：LCD双条带DMA流水线

0.5.7准确批准后唯一USB应用写入、完整保护及180.138秒正常只读启动通过。流水线默认关闭、板上明确启用；在发送当前条带时生成下一条，队列仍1、SPI仍10MHz，新增10KiB内部RAM。相同两组各50回调实测LCD整帧均值Tiny2D 182.330→148.104ms、空present 170.006→140.517ms，两组自行退出并恢复终端；均为含调度/仪表扰动墙钟，非CPU或物理LCD FPS。计时范围有重叠，不跨行相加；LCD目视/颜色/长期压力/异常注入未测。见 [DMA流水线](DMA_PIPELINE.md)。

## 已部署：应用画布条带转换

针对0.5.5设备侧计时中LCD条带转换平均约49–66ms，源码尝试对齐、互不重叠的源和DMA条带按32位双像素进行RGB565字节交换；小端GCC/Clang路径使用may_alias，支持2字节起始对齐，其余对齐、重叠或其他编译器/字节序保留逐像素回退。整幅、条带、不同对齐、重叠别名、边界和哨兵测试通过；目标Xtensa反汇编快路径出现32位载入/写入。0.5.6准确批准并唯一USB应用写入、完整保护和180.198秒只读启动通过；同方法50回调实测Tiny2D条带转换墙钟66.006→50.233ms、LCD整帧197.100→182.330ms，空present条带48.997→39.431ms、整帧178.759→170.006ms。两次短会话含调度/仪表开销，不是纯CPU或物理LCD FPS；LCD目视/颜色未验收。

0.5.5实机两组各50绘制回调、无逐帧打印的设备侧墙钟聚合：Tiny2D/空present的LCD帧197.100/178.759ms、条带转换66.006/48.997ms、提交与等待130.817/129.528ms、画布draw/复制32.216/32.402ms；Tiny2D clear21.208ms/rect0.454ms/text每次1.394ms。各项含调度/仪表扰动，不同任务范围重叠不能跨行相加、不是CPU或目视FPS。具体定义与证据见 [计时诊断](PERF_DIAGNOSTICS.md)。

## 已部署：画布清屏与矩形填充

针对0.5.3组件标记对照，源码改为按行裁剪一次矩形、直接填充像素，并在有GCC/Clang别名扩展时使用对齐的32位双像素写入；2字节对齐开头与奇数像素尾部仍安全处理，其他编译器使用16位回退。颜色、裁剪、画布所有权与显示路径不变。目标Xtensa开发构建的反汇编显示循环为对齐`s32i`而非逐对调用`memcpy`，不等于已证明实际设备加速；PSRAM、并发及DMA仍须实机测量。

新增完整像素参考、前后哨兵、不同对齐、奇偶宽度、颜色、裁剪及整数极值回归；两主机模型各166项CTest、28 Python、4历史mock、full URL/TCP和UART离线构建、格式及纯逻辑静态分析通过。上游已有对齐UBSan诊断未消除。独立候选后经准确hash批准，唯一USB应用写入66.3秒，完整保护和180.025秒只读启动通过。随后按用户请求完成6组各24回调RAM profiling：Tiny2D标记段均值由0.5.3的75.24ms降至59.69ms，周期由187.98ms变为197.29ms（逻辑回调5.32→5.07/s）；清屏标记段49.57→37.66ms，减各自空对照30.90→15.29ms；矩形减空对照19.39→19.25ms。未观察到整体频率提升，矩形无明显收益；短独立会话差异不足以断言性能回归。主机UART墙钟含打印、合批、调度/GC，甚至出现同批两个标记0ms，非纯CPU或物理LCD FPS。TinyGL和LCD目视未测。候选构建时基于未提交工作树/base commit `59eed25`，其源码快照已冻结；后续交互须另请。

## 已部署：应用与LCD目标周期100ms

应用tick帧预算与LCD REPL刷新目标由200ms改为100ms。LCD每帧以各自起点为锚，超时不会积累追帧债务；应用画布背压、双缓冲、SPI 10MHz、GC和超时规则不变。现有320×240 RGB565全屏像素在10MHz SPI下单数据传输理论下限约123ms，尚不含命令、复制及调度开销，因此100ms仅是目标，不承诺10 FPS；FreeRTOS/SPI/DMA、健康状态、画面撕裂及实际速率都需实机验收。控制时钟测试覆盖80ms预算内、120ms超预算、长初始化、背压及按键调度。

此变更已准确批准并随0.5.3唯一USB应用写入66.1秒；完整保护及180.146秒只读启动通过。终端submitted_frames 1@2718ms→1051@179288ms约5.95次/秒，含重复帧，不代表应用动画或LCD目视FPS。启动验收没有自动加载/运行demo；随后按用户请求实际URL加载并profile Tiny2D。各24帧丢弃前2帧后，Tiny2D绘制标记段75.24ms、周期187.98ms（5.32逻辑回调/秒）；空present对照17.65ms、178.29ms（5.61次/秒）。绘制段相差57.59ms，但包含解释器调度/print/USB缓冲，不是纯CPU时间；空对照仍远高于100ms，具体瓶颈尚未单独量化。两组自行退出，TinyGL、LCD目视/撕裂及负载压力未验收。详见 [单槽USB](SINGLE_APP_USB.md)。

随后用户另行允许，按空present/clear/两段text/rect做四组RAM标记对照，各24帧丢前2帧：标记段均值分别18.67/49.57/21.13/38.06ms，较空对照分别增加30.90/2.47/19.39ms；起点周期179.27/187.40/179.65/179.83ms。分组有不同条件分支、print/USB缓冲、GC/调度及PSRAM并发，不是精确CPU时间、差值不可简单相加；初步指向清屏与矩形路径，不能仅据此认定根因或保证优化后10FPS。四组自行退出，未改固件。

## 历史已部署修复：按回调开始计算帧预算（0.5.2）

源码已将下一tick目标从“回调完成时间＋200ms”改为“回调开始时间＋200ms”。计算耗时占用本帧预算，不再额外硬等完整200ms；超预算时只等owner下一轮及可用画布，不补发积压tick。每次新回调重新锚定起点，按键仍可绕过空闲tick等待。64步求值批次、协作式5秒超时、强制GC、显示背压、双缓冲、LCD 200ms周期和10MHz SPI全部保持不变。

**修复已随0.5.2-cadence-usb部署，180.185秒只读启动通过。** 随后用户明确要求重新profile：相同Tiny2D RAM插桩、24帧排除前2帧后，0.5.2绘图span69.68ms、帧间132.68ms、周期202.61ms（约4.94Hz）；空present对照16.33/184.08/200.43ms（约4.99Hz）。Tiny2D周期较旧版降低约39%，逻辑回调率提高约64%，已接近200ms帧预算上限；TinyGL未测，不能推断LCD实际目视FPS。修复前0.5.1的RAM print实测：Tiny2D绘图段72.1ms、帧间段260.5ms、周期332.2ms（约3.01Hz）；空present对照22.7/257.3/280.4ms（约3.57Hz）。均为主机UART标记墙钟，含打印/USB缓冲/解释器让出，不是纯C绘图CPU时间或LCD目视FPS。两组各24帧且自行退出；该结果不能冒充修复后的性能。LCD提交日志约5Hz含重复帧。

新增5项真实解释器/受控时钟回归：80ms回调扣除预算、240ms超预算不追加等待、750ms初始化后不补帧、pending帧背压与重新锚定、按键立即调度。旧代码5项全部失败，修复后通过；完整默认/word32主机模型各165项CTest通过、28 Python和4历史OTA mock、full URL/TCP与APP UART离线构建及格式通过。上游已有对齐UBSan诊断/word32转换告警未屏蔽，不声称sanitizer-clean或Xtensa硬件性能验收。

开发构建`.local/build-cadence-full`／`.local/build-cadence-uart`不是部署产物；随后独立正式候选经准确SHA256 `09148ca37003a3280a50143f386fa6d2390f23819076f648ca6a8ed3d07d216e`／1194720字节／292扇区批准，一次USB应用写入66.3秒，全部保护与180.185秒正常只读启动通过。当前更新/恢复事实见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。全部写权限耗尽；两个demo仍用原URL。部署时未自动运行；随后仅按用户请求加载Tiny2D进行profile，两组已自行退出。

## 0.5.1 部署记录（当时只读启动）

0.5.1帮助改进曾通过180.183秒只读启动；当次启动验收未发送URL/demo或其他REPL命令。后续用户明确请求后实际URL加载Tiny2D并完成RAM profiling、正常退出；LCD未独立目视。该交互仅属0.5.1；0.5.2和0.5.3后来各另做profile，见首节。当前写入与保护事实见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)；以下0.5.0记录为历史。

0.5.0已准确批准并唯一USB应用写入66.2秒，应用/FFpadding/全部补集/8字段/实际完整4MiB保护通过；之后180.101秒只读启动健康，无URL loader启动错误、无UART/TCP/URL/demo命令。0.4.0的RAM demo/定义已随复位清除，未重载，实际切屏/退出及LCD目视仍未验收。当前写入与恢复边界见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)，URL用法见下节。

## 历史0.4.0应用层部署（已被0.5.0替换）

准确批准后，**0.4.0-lisp-app-usb / factory**／989280字节／SHA256 `564c97b824af015d0b475399ce75abd192aa103f37f9a3a30c5f089bf3b68d0b`，在`[0x10000,0x102000)`/242扇区一次USB写入54.1秒。沿用0.3.0表/旧引导，NVS/reserved/free/旧数据及全部范围外字节不变，无OTA/重试/恢复。写前新同板/8字段/实际完整0.3.0基线与旧引导，写后应用/FFpadding/全部补集/8字段及实际完整4MiB均通过。

一次正常RTS启动**180.057秒只读通过**：factory/版本/冻结ELF前缀、ready/align_mod8=0/实际probe PASS、PSRAM128KiB六模式、固定ILI9341帧1→851/心跳0→177；ONLINE8417ms/TCP8487ms，12577ms起34条healthy（最后177577ms），无异常复位/错误。该USB只读验收阶段没有UART/TCP表达式、demo或独立目视；后续demo运行记录见下节，**显示目视/退出仍未验收**。全部Flash/security核验在最终启动前，之后仅UART采集和本地解析。

最新私有`.local/flash/lisp-app-0.4.0/post-protection/current-0.4.0-expected.bin`为重建/实际完整SDK匹配，非新双读。旧0.3.0全片不再代表当前应用；冻结candidate/CONTINUE/resume的PENDING/0次保留历史，实际authorization/marker/execution-*与acceptance-summary另存。全部写权限耗尽，UART释放，无后台监控；交互另批。

源码默认`LISP_APP_ENABLE=n`，本次板上full明确启用y；追加`tests/sdkconfig.lisp-app.defaults`提供画布。根默认版本仍0.3.0，正式候选显式指定`PROJECT_VER=0.4.0-lisp-app-usb`，开发版`0.4.0-lisp-app-dev`不是准确批准镜像。

## URL加载接口（0.5.0部署记录；后续版本已实际加载Tiny2D）

0.5.0启用`app-load-url`，HTTPS下载最多32KiB到RAM，由原owner加载定义；`:load-status`/`:load-cancel`可查/取消，成功后仍显式`app-run`。不写Flash/NVS、不自动run/重定向/重试；仅正常启动已验收，未实际请求URL。以下1023字节限制描述普通REPL，不限制独立URL加载器。完整接口、错误/取消和信任边界见 [APP_URL_LOADING.md](APP_URL_LOADING.md)。

## 历史0.4.0实际demo运行（当时保持画面，0.5.0复位已清除）

用户“运行一下 demo”后，通过TCP一次加载匹配冻结的658字节定义、一次`(app-run demo)`，9秒后普通nil查询被“application owns screen”拒绝，确认应用仍占用前台；随后TCP关闭，未发送q/:stop，demo保持运行。没有USB、串口打开、复位、Flash/NVS/eFuse操作，也没有通用交互回归。实际显示可见性/颜色/FPS和退出恢复仍未验收；此处是逻辑应用模式确认，不冒充助手目视。当前记录另存`.local/flash/lisp-app-0.4.0/demo-run-tcp/summary.json`，不修改原固件冻结/只读验收记录。

当时可在UART按`q`或TCP输入`:key q`退出；助手没有自动退出。后续0.5.0部署的ROM/正常启动复位已清掉该RAM demo，此历史运行不代表当前仍在运行。

## 范围

一个前台 Lisp 应用占用完整320×240屏幕，退出后恢复原40×20终端及提示符。加载定义与运行分开；定义、应用状态和图像均在RAM。仍只有原Lisp owner执行解释器、原LCD owner操作SPI/DMA。

```text
UART / TCP → Lisp owner → 终端快照或应用画布 → LCD owner → 内部DMA条带 → ILI9341
```

不加入窗口管理器、LVGL、后台Lisp任务、文件加载器、SD、持久化、GPIO、BT或DSP。不是恶意Lisp安全沙箱，也不隔离应用对共享全局定义的修改。TCP2323仍无认证，只限可信LAN。

## 应用接口

应用是一个包含三个closure的列表，参数数目必须分别为0、1、0：

```lisp
(define demo
  (list
    (lambda ()
      (progn
        (gfx-clear 0)
        (gfx-rect 20 30 120 60 63488)
        (gfx-text 20 110 "Hello LispBM" 65535)))
    (lambda (event)
      (if (= event 113) 'exit t))
    (lambda () t)))
```

上面的定义不会抢屏。另提交以下表达式才运行：

```lisp
(app-run demo)
```

启动表达式返回`=> t`表示请求已接受，不是`init`/首帧已完成；初始化错误随后写UART日志并恢复终端。

| 回调 | 语义 |
| --- | --- |
| `init()` | 准备首帧；成功完成后才切换到应用画面 |
| `step(event)` | `-1`为tick；其他值为输入字节1–127；返回`'exit`请求退出 |
| `exit()` | 正常停止时调用一次；不能绘图，报错/超时也不能阻止C层恢复显示 |

每次回调独立求值，成功时不输出`=> t`等回调结果。应用生命周期不限于5秒，但每次回调仍有**协作式5秒超时**，每批64步、owner每轮让出一个tick。它不是可抢占原生操作的硬实时保证。应用不允许遗留detached contexts；超时/遗留contexts清全部RAM定义，再请求恢复终端。`init`错误不会提交首帧，也不执行`exit`；`step`错误会请求停止并尝试`exit`。

`(app-stop)`在回调中请求停止，不是非局部跳出；当前回调需要返回。正常退出保留用户REPL定义，包括全局`demo`；临时内部descriptor解除绑定，后续GC回收无其他引用的数据。内部`__wrover_app`为GC根，回调每次重新查找并校验，不能依赖未root的C引用；用户应用不要修改该保留绑定。

## 绘图

```lisp
(gfx-clear color)
(gfx-pixel x y color)
(gfx-line x0 y0 x1 y1 color)
(gfx-rect x y width height color)
(gfx-text x y "ASCII text" color)
(gfx-present)
```

- 只允许在`init`/`step`内调用。坐标为有符号32位范围内的整数，颜色为RGB565整数0–65535；浮点和溢出参数拒绝。
- 左上角为(0,0)，`gfx-rect`为实心矩形；坐标裁剪，非正宽高不绘制，线段先裁剪再光栅化，不按巨大离屏坐标循环。
- 文字复用现有8×12 ASCII字形，透明背景，最多128个可打印ASCII字符；不接受二进制数组冒充字符串或中文/换行。没有旋转/中文字体/图片解码。
- 绘图是**回调级事务**：成功结束后自动提交有修改的画布；`gfx-present`只是标记本次需要提交，不立即刷屏、不阻塞等待LCD。报错/超时不会提交半成品。
- 每次取得后画布先复制当前前画布，因此增量绘图不会因双缓冲而丢掉上一帧变化。

两份RGB565画布合计 **307200字节（300KiB）**，由C在启用配置的启动阶段一次性分配到PSRAM，固定活到重启，不进入约64KiB的LispBM通用内存池。分配失败保留普通终端，不挤占内部DMA堆、不自动重试。

一次最多一个待提交帧。发布后owner不能复用它；LCD在帧边界接管新前画布、释放旧前画布，再通过已有内部条带完成DMA提交。元数据短锁和像素复制/绘图分开，锁内没有整帧拷贝或SPI等待。退出需确认新一代终端帧实际提交成功，旧帧完成不能冒充恢复；DMA失败保留资源隔离，不自动恢复/重试。恢复等待3秒超时会明确报告未确认，不能声称屏幕恢复；解释器初始化失败也不能声称UART REPL可用。

LCD沿用固定ILI9341、方向和10MHz SPI；板上0.5.7应用/LCD目标周期均为100ms，见首节。空闲tick以回调开始为基准，计算与背压可能降低实际频率，不在计算结束后额外等满整周期，也不积压补帧。全屏像素理论传输约123ms，0.5.3的Tiny2D逻辑回调实测见首节；0.5.4逻辑回调复测见首节，物理LCD FPS未测。双缓冲避免LCD读取正在绘制的画布，**不保证玻璃屏扫描无撕裂**。

## 输入、日志和退出

- 应用期间UART字节不进入终端编辑器；`q`是否退出由应用决定。UART Ctrl-C统一请求停止当前应用，终端模式仍保留原Ctrl-C清RAM行为。切回终端时丢弃UART已缓冲的旧应用输入，避免误当Lisp源码。
- 输入队列固定16字节，超限明确报错并停止应用，不悄悄执行截断源码。当前只是字节事件，不是完整键盘/组合键协议。
- TCP使用`:key q`发送一个可打印字符，`:stop`请求停止；普通Lisp求值、`:reset`/`:clear`在应用期间拒绝。停止后再正常求值或`:reset`。
- `app-run`的普通启动表达式完成后立即回复，**不等整个应用退出**；控制命令也立即回复接受/拒绝，独立于原8秒请求等待。应用回调和停止清理继续由owner推进。
- TCP EOF/断开不结束应用；重新连接可发送`:stop`。没有认证、自动重试或新增监听端口。普通TCP前端的Ctrl-C仍只清网络输入，不能替代`:stop`。
- 应用`print`只写UART日志，不修改隐藏的终端，也不异步推送给TCP客户端；每回调print额度512字节，超出提示截断。SDK日志仍可能穿插UART输出，不显示在应用画面。
- 正常停止先等待当前回调返回，再尝试`exit`，最后请求终端帧；坏回调各自最多等待上述5秒协作式期限。`:stop`回复“accepted”是请求接受，不是退出/屏幕恢复已完成。

## 示例与加载边界

[`examples/lisp/demo.lisp`](../examples/lisp/demo.lisp)是658字节、单个`define`表单（注释放在已打开的表单内部，避免REPL先提交独立注释行），未自动运行。它画矩形、线和文字；普通按键显示反馈，`q`返回终端。

[`examples/lisp/tinygl.lisp`](../examples/lisp/tinygl.lisp)是TinyGL风格的软件3D线框立方体，**不是**TinyGL库或OpenGL实现。它用32项Q10正弦表、整数投影和现有`gfx-line`绘制；不需要浮点/三角函数扩展。加载后提交`(app-run tinygl)`；`p`或空格暂停/继续，`q`退出，TCP对应`:key p`／`:key q`，`:stop`仍可停止。定义仅在RAM；额外的`tinygl-*`全局定义在正常退出后保留，`:reset`或重启清除。

[`examples/lisp/tiny2d.lisp`](../examples/lisp/tiny2d.lisp)是配套的最低复杂度动画基线：每帧只调用`gfx-clear`、一个`gfx-rect`和两段固定文字，没有`map`、顶点列表或3D投影。它与TinyGL使用相同的应用回调、双画布和完整320×240 LCD提交路径；`(app-run tiny2d)`启动，控制与TinyGL相同。若它在板上也约1 FPS，瓶颈不是TinyGL几何运算；若它明显更快，再优先分析TinyGL的LispBM分配/GC。该比较仍不能单独量化LCD、DMA和回调时间，需固件遥测才可归因。

可用现有REPL续行输入加载`demo`定义，完成后单独提交`(app-run demo)`；普通输入仍限1023字节，不要粘贴多条独立提交期待自动排队。`tinygl.lisp`和1096字节的`tiny2d.lisp`都超过此限制，需通过已启用的最多32KiB HTTPS URL→RAM loader加载，见上节；没有`:load`本地文件命令或电脑端文件上传器，不读写Flash数据分区。新增主机集成测试实际经该loader加载TinyGL/2D源码，逐帧和独立整数投影预期比对首帧/32相位、暂停、TCP/UART停止、八次重启式启停及256帧压力；这不是设备、LCD目视或网络HTTPS验收。

## 离线构建与测试

新目录，不覆盖任何已部署或冻结构建：

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-lisp-app-full \
  -DSDKCONFIG="$PWD/.local/sdkconfig.lisp-app-full" \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;tests/sdkconfig.psram.defaults;tests/sdkconfig.lcd.defaults;tests/sdkconfig.lisp-repl.defaults;tests/sdkconfig.lisp-app.defaults;tests/sdkconfig.wifi-sta.defaults;tests/sdkconfig.wifi-repl.defaults" \
  -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt \
  -DCCACHE_ENABLE=0 -DPROJECT_VER=0.4.0-lisp-app-dev build

cmake -S tests -B .local/host-build-lisp-app -G "Unix Makefiles" -DENABLE_LISPBM_TESTS=ON
cmake --build .local/host-build-lisp-app --parallel 4
ctest --test-dir .local/host-build-lisp-app --output-on-failure
```

默认64位与额外value32/pointer64模型各 **138项C ASan/UBSan通过**；相对于原96项新增42项：6画布/光栅/控制、23真实Lisp应用用例、1新增初始化故障、1并发画布、1socket/owner模型、5项TinyGL URL加载/首帧与相位/控制/启停/压力测试，以及相同5项2D基线测试。包括12秒模拟应用寿命、初始化/求值/退出错误和超时、GC/匿名closure/50次启停、增量帧、输入溢出、UART Ctrl-C、原终端保留、加载不自动运行、生产TCP session的真实socketpair启动及时回复/拒绝源码/key/EOF/复连/stop。

并发测试用两个主机线程检查50次启动/恢复中的整帧不可变；网络测试用真实LispBM和生产session，但邮箱/调度为主机模型，不是FreeRTOS/lwIP/ESP32证据。额外word32模型仍使用已有Rosetta/低地址测试分配，不等价于目标ABI；新TCP测试最初因严格POSIX宏隐藏macOS `MAP_ANON`而编译阻断，已在该测试声明Darwin扩展后通过，无设备访问或固件重试。

**28 Python、4历史OTA mock、六配置编译、clang-format与四个纯逻辑文件Clang分析通过。** 六配置为单槽应用TCP、应用UART、应用关闭UART、全部功能关闭、LCD扫描，以及显式历史双槽TCP/OTA（应用关闭）。保留上游既有unused/longjmp警告和word32模型转换警告。完整开发镜像989280字节，余1763232字节；非冻结候选，版本/容量不是写入许可。

0.5.7已实机通过USB应用更新、正常启动/probe/健康，并经TCP实际加载和运行Tiny2D、自行退出及恢复终端；这不等于独立LCD目视。UART控制、TCP按键/中断、PSRAM画布压力、输入边界及LCD目视/FPS/颜色/撕裂仍 **NOT TESTED**。旧版本交互不能挪作本版证据。全部写权限耗尽，新固件须新准确hash/factory实际范围批准＋新同板/8字段/当前0.5.7实际完整4MiB基线＋新marker单次USB，不重试/恢复/表/引导/NVS/旧数据变化。
