# Wi-Fi REPL：直接TCP，无证书/token（当前0.4.0 factory，已部署）

0.4.0-lisp-app-usb已准确批准并唯一USB应用写入54.1秒，沿用单槽表/旧引导。180.057秒正常启动只读通过：ONLINE8417ms/TCP8487ms、版本/IP/nc提示各一次、实际probe PASS与联网后34条healthy。网络仍TCP2323，无登录/自动eval/HTTPS/OTA；本次没有UART/TCP交互或demo，旧0.2.3 nc测试仅历史。见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。

当前启用Lisp应用层：终端模式仍如下REPL，应用运行期间普通源码拒绝，TCP :stop/:key X为控制命令，断线不停止应用，详见 [LISP_APPS.md](LISP_APPS.md)。这些新交互目前仅主机测试，未实机验收；全部写权限已消费，后续交互另批。

以下0.2.4/旧OTA说明为历史，旧接口/IP参考和冻结PENDING不能当作当前操作任务或权限。

## 历史安静启动提示（0.2.4已部署并只读验收）

已在新源码加简短LCD/串口启动提示：版本、USB115200、Wi-Fi connecting；联网且TCP就绪后一次性报告IP、TCP2323、`nc IP 2323`、OTAHTTPS8443。启动不再自动打印整段help，手动`:help`仍可用。内部单次(+1 2)==3健康probe保留但成功静默，不把测试输出刷到LCD；失败仍停止健康确认。

之前批量eval来自主机验收脚本，不是0.2.3固件自动执行；这些helper现默认拒绝执行。**以后启动观察仅只读采集，不自动发送UART/TCP eval**，若未来另要交互测试需明确要求，不能把刷机批准当测试输入许可。0.2.4已准确批准并唯一POST部署B；最终180.146秒只读采集、无UART/TCP eval，旧归档不改。

主机新增4项banner格式/边界和实际probe静默断言，两模型各92项、15Python/4OTA mock、五配置/格式/纯逻辑分析通过；实机日志已确认版本/USB/IP/端口/nc各一次、无eval和自动help；LCD整串已提交但未独立目视。共享UART的初始connecting行被SDK记录插入，去掉仅timestamped SDK记录后匹配；最终IP/端口/nc原始行完整。

新候选私有冻结 `.local/flash/boot-info-2026-11-10/`：0.2.4-boot-info-ota1106720字节，SHA256 `a068d068924155ee7f069548ad83f6d37b5cdacce7e7c3ebeaa2e3008a794fc2`，B镜像 `[0x160000,0x26e320)`、擦写 `[0x160000,0x26f000)`/271扇区，另仅SDK控制 `[0xc000,0xe000)` 生命周期更新。114个来源文件构建前后相同、镜像header/footer及冻结SHA256SUMS通过，后来用户准确批准，已唯一POST/HTTP200，38836ms完成30秒健康确认，保留A0.2.3与补集；所有写权限耗尽。最终只读观察，不自动UART/TCP eval。冻结PENDING原文保持不改，实际结果见本目录acceptance-summary/resume。

## 直接使用

```sh
nc DEVICE_IPV4 2323
```

当前IP仅私存 `.local/flash/lisp-app-0.4.0/device-ip.txt`，DHCP可能改变，不发布IP/MAC。直接出现 `lbm>`，输入：

```lisp
(+ 1 2)
(define twice (lambda (x) (* x 2)))
(twice 21)
(print "hello")
```

结果为3、42、hello，输出同时交给原UART/LCD；定义保留在共享RAM，不保存Flash。`:help`、`:clear`、`:reset`沿用；`:reset`清全部RAM定义。未闭合表达式显示`...>`，支持分包、多行、CR/LF/CRLF、退格/Ctrl-U；Ctrl-C清当前网络输入（不抢占已提交的求值），清定义请用`:reset`。结束nc即断开，不发送EOF残留源码。不是Telnet协议，不要用Telnet协商客户端。

**任何能连接该端口的人都能执行Lisp，没有保密性和身份校验；只用于可信局域网，不做公网端口映射。** 本次不添加Lisp系统命令、任意内存/网络/GPIO/文件/DSP接口，也不是恶意Lisp程序安全沙箱。

## 历史0.2.3实机交互验收（2026-11-10，不能挪作0.2.4结果）

- 用户准确批准上述0.2.3 hash/A窗口及SDK控制记录，新的同板/8字段/实际完整0.2.2基线通过；先采集观察B/probe/HTTPS就绪再最终TLS检查，才消费marker。唯一OTA POST/HTTP200写1106176字节，软件重启A，38806ms完成30秒健康确认；无重传/USB镜像写入。
- 正常A重启180.049秒，12项UART＋24项真实明文TCP通过，包括实际`nc`（没有TLS/token/登录）、3/42/hello/定义/错误/reset、分包/多行/CRLF编辑、NUL/超长拒绝与恢复、UART busy且未执行、半句EOF不执行、重连后RAM定义保留。
- LCD提交帧1→851、心跳日志0→179、PSRAM独占128KiB六模式通过；最终HTTPS状态A/版本/SDKhash/online/healthy/VALID匹配。LCD提交不是目视/滚动验收。UART已释放，无后台监控。
- A+FFpadding、3补集（B0.2.2/引导/表/NVS/free/A尾/旧数据）、8字段和实际seq5/4 VALID/CRC、完整4MiB重建预期匹配，B控制扇区不变。**所有Flash/security核验在最终正常A180秒测试前；之后只读HTTPS，非新双读备份。**

## 最小实现

- 一个TCP任务、一个客户端，原Lisp owner继续独占求值。无TLS、token、HTTP头/状态码、WebSocket、浏览器、用户/会话/历史系统。旧HTTPS `/repl` 路由和公开的BB客户端已移除。
- 源码复用原输入扫描器；累计最多1023字节。溢出、NUL/二进制输入整次拒绝，绝不把忽略/截断后的片段当代码；断开时未提交输入丢弃。只在完整表达式Enter后提交。
- 请求经原固定邮箱给Lisp任务，不把socket栈指针交给异步owner。UART已有未完成输入或正在求值时返回文本`ERROR: REPL busy`，不混拼源码；网络求值期间不混入UART字节。
- 仍沿用已有2KiB有界回复、5秒协作式求值/8秒owner等待和超时隔离。回复过长有截断提示；丢连接可能已经执行，不自动重发。不是所有原生操作都保证可抢占。
- 不为每个客户端创建任务，空闲连接可一直占用唯一会话；断开后接受下一连接。socket发送有5秒超时；发送失败只关当前连接，监听失败停止/不健康，不自动反复启动。
- Lisp任务在Wi-Fi初始化前启动，TCP任务须等原Wi-Fi ONLINE再创建socket；监听就绪参与原REPL健康，保持OTA30秒确认/120秒未确认回退，不绕过。没有NVS/新GPIO/网络设置变化。

## 测试与构建

- 默认64位和额外value32/pointer64模型各 **88项C ASan/UBSan** 通过。TCP8项使用真实主机socketpair运行同一生产session代码：基本会话、分包/续行、编辑/CRLF、溢出、二进制、半句EOF、busy、timeout；另1项真实LispBM经socket会话验证3/42/hello/错误恢复/重置。**这是主机socket，不是ESP32/lwIP/FreeRTOS实机验收。**
- 15项Python、4个OTA uploader mock、格式/Clang纯逻辑分析通过。默认关闭/扫描/UART+OTA/TCP+OTA，以及TCP不启用OTA且不传任何cert/token/key输入，五配置编译通过。
- `WIFI_REPL_ENABLE`仍默认n，现在只依赖 `LISP_REPL_ENABLE && WIFI_SERVICE_CONNECT_ENABLE`，不依赖OTA。去掉REPL证书/token没有改OTA原密钥；部署候选必须保留OTA与rollback，不能拿不带OTA的回归构建上传。
- 新构建使用独立 `.local/build-tcp-repl-*`，不覆盖0.2.2/0.2.1冻结产物。含Wi-Fi/OTA秘密的bin/ELF/对象只留私有 `.local/`，目录0700、日志0600、冻结0400，禁用ccache。
- 真实TCP连接/普通EOF重连/半句丢弃、UART未完成输入busy保护与OTA确认已通过上述有限路径；**NOT TESTED**：LCD目视/滚动、任意并发/慢客户端、真实Wi-Fi掉线/丢ACK、owner8秒隔离故障注入、长GC/full8MiB/10分钟/受控断电/新回退故障注入。主机证据与真实设备结果分开记录。

## 历史0.2.3冻结镜像（旧双槽阶段，授权耗尽）

`0.2.3-tcp-repl-ota`，1106176字节，SHA256 `30e3d441688931e76e5878417f333e8140ae05ff91a3076e208ca7569cf33941`；A镜像 `[0x10000,0x11e100)`、擦写 `[0x10000,0x11f000)`/271扇区，另仅SDK控制 `[0xc000,0xe000)` 生命周期更新。A比旧0.2.1擦除窗口多一个扇区，仍不越槽/不碰B。

私有冻结 `.local/flash/tcp-repl-2026-11-10/`：111个来源文件构建前后相同、SDK镜像头/footer与SHA256SUMS通过，app/ELF/sdkconfig/source0400。后来用户对该准确hash/范围明确批准，已创建authorization/marker并完成唯一POST；冻结candidate/source保留原PENDING历史，当前事实见acceptance-summary/resume，不改档案、不作为再次写许可。

## 历史0.2.4设备与下一次OTA边界（不适用当前0.4.0）

历史0.2.4阶段运行B/VALID并保留A0.2.3备用；当时检查点 `.local/flash/boot-info-2026-11-10/`，先读CONTINUE/resume/acceptance-summary/final-boot-summary/post-protection/summary及authorization/marker（本版没有tcp-tests/交互eval）。最新参考 `post-protection/current-0.2.4-expected.bin` 是重建且实际完整SDK匹配，不是新双读；核验在最终B180秒前。旧0.2.3及更早全片预期不再代表当前B/控制，不改旧归档。

全部写权限已耗尽。下次新镜像须新准确hash/范围批准，目标非运行 **A**，会替换备用0.2.3，保留当前B及补集。仍须新同板/8字段/实际最新4MiB基线，先采集等就绪再最终TLS，最后唯一POST；失败停止，不重传、不USB恢复。DSP/蓝牙仍未加入。
