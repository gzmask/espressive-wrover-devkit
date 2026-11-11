# URL → RAM 应用加载（当前0.5.7 factory）

0.5.7已部署并通过180.138秒只读启动，该次验收没有发送URL、demo或其他REPL命令；随后按用户请求实际HTTPS URL加载Tiny2D并完成两组设备侧RAM计时。该有限成功不外推证书失败、断网或其他URL边界。当前写入与保护事实见 [单槽USB](SINGLE_APP_USB.md)。下述0.5.0部署记录为历史，旧全片参考不代表当前应用。

准确批准`5a8bb890f611dfb274f9b4a5d284654ace2307db994483652c9acdb287b57eea`／1194400字节／`[0x10000,0x134000)`／292扇区后，一次USB应用写入66.2秒，未写表/引导/NVS/旧控制/eFuse。新同板/8字段/实际完整0.4.0基线/旧引导及写后应用/FFpadding/全部补集/8字段/实际完整4MiB均通过。

**180.101秒正常启动只读通过**：factory/0.5.0/冻结ELF前缀80c5d66a7、ready/align_mod8=0/probe PASS、PSRAM128KiB六模式、固定ILI9341帧1→851/心跳0→178；ONLINE8508ms/TCP8568ms，34条联网后healthy，无URL loader启动错误。没有发送UART/TCP/URL/demo命令，真实HTTPS加载/校时/加载后运行仍未验收；RAM demo/定义已随复位清除。

当时私有检查点`.local/flash/app-url-0.5.0/`的execution-*和acceptance-summary记录实际执行；冻结candidate/CONTINUE/resume仍保留PENDING历史，不修改。全部写权限耗尽，UART释放，无后台监控；交互另批，不自动commit/push。所有Flash/security核验在最后启动前，此后仅UART采集和本地整理。

## 使用

启用 `LISP_APP_URL_ENABLE=y` 后，在终端模式提交：

```lisp
(app-load-url "https://raw.githubusercontent.com/gzmask/espressive-wrover-devkit/26315da7abf8ec9b8bfb9e46720e3e738d8edd19/examples/lisp/demo.lisp")
```

返回 `=> t` 只表示启动请求接受，**不是下载或加载成功**。随后在 UART／TCP REPL 使用控制命令：

```text
:load-status
```

状态为 `idle / queued / downloading / loading / loaded / failed / cancelled`。看到 `loaded` 后，再单独提交：

```lisp
(app-run demo)
```

加载器不自动运行应用，下载的顶层代码也不能调用 `app-run` 或递归 `app-load-url`。应用正在运行时先 `:stop` 或 `:key q` 并等待终端恢复，再加载 URL；回调不能发起下载。`demo` 的普通退出保留定义，重启、掉电或终端 `:reset` 后消失；没有安装到 Flash、缓存、开机恢复、依赖管理或包仓库。

示例 URL 固定到已公开的源码 commit，而不是浮动 `main`；此demo URL未请求，0.5.7另已实际加载固定commit的Tiny2D作有限诊断，不外推任意URL。下载的是纯文本 `.lisp` 内容，不是 GitHub 的 HTML 文件页面。URL 最多512字节，只支持小写 `https://`、DNS/IPv4主机、默认443或显式`:443`及非空路径；拒绝HTTP、用户信息、片段、空白、非ASCII、反斜线、IPv6字面量和其它端口。不支持私有CA、自签名站点、HTTP认证或自动跳转；应直接给最终 HTTPS raw URL。不要在 URL 中放 token／密码；原无认证 TCP 会回显提交源码。

## 取消与错误

```text
:load-cancel
```

- **下载阶段取消/网络或校验失败**：完全不执行下载源码，既有定义保留。UART Ctrl-C 同样取消。TCP断开不会取消请求，重新连接可以查询或取消；不自动重发GET。
- **求值阶段取消、超时或遗留 detached contexts**：停止解释器中的全部 contexts，清空所有RAM定义；不是只撤销这一个文件。UART Ctrl-C也走此路径。
- **普通语法/求值错误**：状态 `failed`，恢复普通REPL；已经成功执行的顶层定义/修改可能保留。**不是事务回滚、命名空间隔离或安全沙箱。** 失败后不要把部分定义当成成功安装的应用。
- 加载期间普通UART/TCP求值被拒绝，`:load-status`／`:load-cancel`仍立即响应；不占用原8秒TCP请求等待到下载结束。异步结果写UART/LCD终端，不跨TCP连接推送。
- `:load-cancel`回复描述取消阶段，`:load-status`保留最近结果；`:reset`清定义也会重置加载状态。取消下载不会抢先回收网络任务仍在用的缓冲，新请求可能暂时报 `failed`，需等旧worker真正结束后手动重新请求，不自动重试。

## 所有权、资源与期限

```text
(app-load-url ...) → 原 Lisp owner 排队 → 单 URL worker
                                         ↓ HTTPS → 有界 PSRAM 缓冲
原 Lisp owner ← 完整/成功结果 ← 固定邮箱
    ↓ 独立复制、解析/执行顶层定义
显式 app-run → 原应用调度器 → 原 LCD owner / DMA
```

- 只有原Lisp owner调用LispBM，网络任务不执行解释器或操作LCD。一个下载/加载在途，不为每次请求创建线程；空闲worker只等通知，不在启动时发网络请求。
- 下载最多 **32768字节**；空正文、截断、长度不符、超限、NUL、非ASCII和除TAB/CR/LF外的控制字节均拒绝。只接受HTTP200；压缩、非200及重定向拒绝，未知长度/chunked也须SDK确认完整。整个正文验证通过前不解析、不求值。
- 下载缓冲固定 **32769字节PSRAM**；解释器独立的 **32769字节源码区**也在PSRAM，不占LispBM通用64KiB内存池。owner复制后才释放邮箱租约。普通REPL仍限1023字节，不能用长粘贴绕过；32KiB是传输/源码上限，不保证任意该大小程序都能放进解释器堆。
- 网络目标总期限30秒（含首次校时），每次阻塞IO最多配置3秒，首次校时等待最多10秒；owner在约35秒后也会独立终止等待。DNS/TLS/SDK内部阻塞不是可抢占的硬实时保证；超时/取消后不复用worker仍拥有的内存，迟到结果丢弃。网络阻塞不阻塞原Lisp心跳/LCD任务。
- 顶层加载继续使用原 **5秒协作式求值期限、64步批次、输出额度、无detached contexts** 规则。应用自己的生命周期/回调限制不变。
- 分配/任务创建失败时普通REPL继续，不自动重建loader或挤占内部DMA堆作PSRAM回退。原Wi-Fi、GPIO、显示方向、NVS、分区、固件更新方式不变；HTTPS仅作为出站客户端，不新增监听端口或OTA。

## HTTPS 信任边界

使用ESP-IDF公开CA证书包，校验证书链、URL主机名与证书有效期；没有 insecure/跳过CN/HTTP降级路径。首次明确加载请求且时钟未设时，通过 `pool.ntp.org` 做一次限时SNTP校时，结束即停止，不新增持久后台同步或写NVS。SNTP没有认证，不等于可信硬件时钟；HTTPS也不是代码签名、固定内容hash或恶意Lisp隔离。只加载你信任的来源/固定commit，原TCP2323仍限可信LAN。

## 离线构建

使用新目录，不覆盖 `.local/build-lisp-app-candidate` 或任何已部署/冻结产物：

```sh
umask 077
export IDF_TOOLS_PATH="$PWD/.local/espressif"
. .local/esp-idf/export.sh
idf.py -B .local/build-app-url-full \
  -DSDKCONFIG="$PWD/.local/sdkconfig.app-url-full" \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;tests/sdkconfig.psram.defaults;tests/sdkconfig.lcd.defaults;tests/sdkconfig.lisp-repl.defaults;tests/sdkconfig.lisp-app.defaults;tests/sdkconfig.wifi-sta.defaults;tests/sdkconfig.wifi-repl.defaults;tests/sdkconfig.app-url.defaults" \
  -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt \
  -DCCACHE_ENABLE=0 -DPROJECT_VER=0.5.0-app-url-dev build

cmake -S tests -B .local/host-build-url -G "Unix Makefiles" -DENABLE_LISPBM_TESTS=ON
cmake --build .local/host-build-url --parallel 4
ctest --test-dir .local/host-build-url --output-on-failure
```

`LISP_APP_URL_ENABLE` 默认n，依赖原app、Wi-Fi station；启用时要求HTTPS、CA包和证书日期检查。未启用的配置不编译/启动URL worker，`app-load-url`返回不可用错误；Lisp运行时仍预留独立长源码区。根默认版本仍0.3.0，不把开发版本标签当板上版本或写入许可。

## 证据与未测项

两主机模型各 **149项C ASan/UBSan**：相对0.4.0的128项新增7个纯下载/URL/邮箱、13个真实Lisp加载用例和1个初始化故障。覆盖32KiB边界、超过1023字节的定义、借用缓冲释放后覆盖、错误/取消/迟到结果/重置/超时/GC重复加载、禁止嵌套/自动run。扩展原真实socketpair/生产session测试，验证URL及时ACK、状态、拒绝求值、EOF继续/重连、取消和原定义保留；网络下载和邮箱调度仍为主机模型，不是真实HTTPS/lwIP/FreeRTOS验收。word32/pointer64使用已有Rosetta模型，不等价目标ABI。

28 Python、4历史OTA mock、六配置编译及格式/纯逻辑分析通过。六配置为URL+TCP、URL+UART、URL关闭/app+UART、全部关闭、LCD扫描、显式历史TCP/OTA（app/URL关闭）。完整开发镜像约1.14MiB，非冻结候选。依赖预扫描/Kconfig首次编译阻断及`poll`命名冲突都仅为本地主机构建问题，已修正，无设备访问/重试。

**未实测**：ESP32出站TLS/证书失效/错误主机、真实DNS/SNTP/重定向/chunked/截断/慢服务端/断网、目标堆/栈峰值、加载后LCD应用切换和退出。接口已随0.5.0部署，但本次仅验证正常启动；不把主机结果、旧demo或启动健康当成URL硬件验收。
