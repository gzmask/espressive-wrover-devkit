# Lisp 数据 API：HTTPS / JSON / 时间 / 字符串

该 API 为**离线实现，尚未部署**。起始基线 `a17b5d9f6fccfcffff7d49cc101f95d2c93d8534` 已核对 origin/main；板上最后记录仍为 0.5.12，不因开发构建而新增这些接口。没有设备/实卡/真实天气 API 请求、固件冻结或写入许可；不开机自动运行应用。这一 API 开发阶段不含天气页面；后续离线天气源码与限制见 [WEATHER_APP.md](WEATHER_APP.md)，同样尚未部署。

目标是让 Lisp 自行选择 Open-Meteo 字段、处理天气码与刷新策略，而不是固件增加供应商专用 weather-fetch。源码加载与数据下载严格分开：HTTP 正文**从不求值**，不更改 `app-load-url` 的代码加载契约。

## HTTPS GET

需要显式 `CONFIG_LISP_HTTP_ENABLE=y`，默认关闭，依赖 Lisp REPL 和现有 Wi-Fi station。追加 `tests/sdkconfig.http.defaults`；HTTP 可在 URL loader 未启用时独立启用。不启用 HTTP 时 JSON、字符串和时间查询仍可用，http-get 返回 `(error unavailable)`。

```lisp
(define request (http-get "https://api.open-meteo.com/v1/forecast?latitude=0&longitude=0&current=temperature_2m,weather_code&forecast_days=1"))
;; (ok id) 或 (error reason)。这里的坐标只是示例，不是用户位置。

(define id (ix request 1)) ;; 仅在 (ix request 0) 为 'ok 时
(define response (http-poll id))
;; (pending)
;; (ok status body)
;; (error reason status)

(http-cancel id)
(http-release id)
```

- 只有一个槽；请求与结果未释放时新的 GET 返回 `(error busy)`。运行中应用的 init/step 回调可调用，不阻塞等待网络。普通 REPL 的 app gate 不变。
- **先检查返回标签，再访问字段。** GET 错误为两元素列表；poll 错误为三元素列表，尚无 HTTP 响应时 status=0。网络错误是普通数据，不会单独终止应用；错误参数仍是 VM type_error，真正内存不足仍可能触发 GC/失败。
- poll 不消费终态；重复 poll 返回相同结果，但成功时每次都会复制一份正文进 Lisp 内存。应用通常完成后只 poll 一次、提取需要的数据并 release。release 后该 id 永久失效，poll 返回 `(error stale 0)`；旧 id 不能取消新请求。
- cancel 标记取消；在途 SDK 调用可能尚未返回，poll 可继续 pending。release 在途请求会取消并丢弃结果，但不能提前复用 worker 的缓冲：返回后新 GET 仍可能 busy，直至旧 worker 实际退出该请求。无自动重试或新 worker。
- VM reset 释放/取消其请求；应用退出会释放**由该应用发起**的请求。REPL 发起的请求不会仅因之后运行的应用退出而自动释放。迟到结果不写 Lisp、不重建定义。
- 接受 GET 后立即派发，**不是顶层表达式事务**。随后其他 Lisp 代码报错不撤回已派发请求；显式取消或 reset。接收前预分配 ACK，OOM/GC 重试不重复发起网络请求。
- 只有原 Lisp owner 分配/读取 Lisp 值；worker 不持有 Lisp 指针、closure 或栈指针。worker 的 READY 正文租约直到 release；poll 先复制，复制/分配失败不消费结果。

### 请求边界

- 只支持 HTTPS、IPv4/DNS、443 端口，URL 最多 512 字节且包含路径；沿用源码 loader 的纯 URL 校验。可有 query；拒绝 userinfo、fragment、空白、NUL、IPv6 和其他端口。
- 首版只有 GET，固定 Accept 为 JSON/text、User-Agent 为 wrover-lisp/0.6、Accept-Encoding=identity。无自定义 header、API Key/Bearer、Cookie、POST、重定向或 gzip。Open-Meteo 的免 Key 接口无需先引入认证配置。
- 完整正文最多 **8192 字节**，接受 2xx；非 2xx 返回 http-status 并保留状态码，不返回其错误正文。未知长度/chunked 必须由 SDK 确认完整；声明长度不符、超限、NUL、压缩正文拒绝。不解析图片/二进制文件，不依赖 Content-Type 来保证安全。
- 30 秒目标总期限、配置的单次 IO 最多 3 秒。DNS、TLS、SDK 内部阻塞与 cleanup **不是可抢占的硬实时保证**；cancel 后不确定返回的 worker 保持原缓冲所有权。cleanup 失败令该服务 unavailable，不重建或重试。
- 常见 reason：unavailable、offline、busy、network、timeout、cancelled、http-status、too-large、truncated、encoding、invalid-text、clock、stale、exhausted。DNS/TLS/连接等底层失败暂统一 network，**不伪称能精确区分**。ID 为不循环的 24 位递增值；耗尽后拒绝新请求直至重启，避免旧 ID 别名。
- TLS 校验 CA、主机名、证书日期；禁止 insecure 和 HTTP 降级。时钟未设时调用一次有界 SNTP，与源码 loader 共用串行协调器，不再由两个 worker 各自初始化 SNTP。没有周期后台校时；SNTP 不是认证时间源。
- 无新的入站端口、Flash/NVS 写入、网络配置或持久缓存。该 API 是可信程序的联网能力，**不是沙箱**或出站目的地主机白名单；原无认证 TCP 仍只适用于可信 LAN。不要在 URL 中放密钥；REPL 仍会回显提交源码。

## JSON

```lisp
(define parsed (json-parse (ix response 2))) ;; 先检查 response 为 'ok
;; (ok value) 或 (error invalid-json/too-large/depth-limit/node-limit)
(define data (ix parsed 1))                ;; 先检查 parsed 为 'ok
(json-get (json-get data "current") "temperature_2m")
```

对象表示为 `(json-object (key . value) ...)`，key 为字符串，**不会将远端键名 intern 成符号**。数组为 `(json-array value ...)`，保持原顺序。

`(json-get object "key")` / `(json-get array index)`：缺失返回 json-missing。null/false/true 分别为 json-null/json-false/json-true，空对象/数组也各保留类型，均不混同 nil。这些符号包括 json-false 在 Lisp 中是非 nil 值，必须显式比较，不能直接当 Lisp 布尔值。重复的解码后 key **最后一个值生效**。容器是普通 Lisp 数据，可修改，不是不透明/防篡改对象。

限制：
- 输入最多 8192 字节，256 个节点（对象 key 也计数），根深度 0、最大深度 12；单个解码字符串最多 1024 字节。
- 严格 JSON 语法，拒绝尾随内容/逗号、NaN/Infinity、数字语法错误；整数须可表示为 int64，实数须可表示为有限 double（包括下溢的 ERANGE 也拒绝）。
- 验证 UTF-8，解码 Unicode escape/代理对；拒绝无效序列和 U+0000。转义的其他控制字符可以解析，但 gfx-text 不接受它们。
- 同步、有界、无 IO。整个文档语法通过后才构造 Lisp 值；不使用 Lisp read/eval，也不使用全局 cJSON allocator hook。
- 沿用项目约 64KiB 的目标 Lisp 通用内存池。8KiB 传输上限不是“任意此大小 JSON 一定能解析”的内存保证。尽量只请求一天和少量字段，及时去除原始正文/旧文档引用。
- 空字符串的上游打印可能显示 `[0]`；它仍是 NUL 结尾的空 byte-array。较大正文/文档在 REPL 自动打印时可能显示 `<result too large>`，不代表绑定或解析失败；查询返回标签/具体字段。

## 时间

| API | 返回 |
| --- | --- |
| (monotonic-ms) | 本次启动单调毫秒 u64，适合刷新间隔、缓存年龄 |
| (time-unix) | UTC Unix 秒 i64，未设时 nil |
| (time-status) | set / unset |

单调时间取原 owner 本轮时钟，**不是回调次数**，跨 32 位毫秒边界仍保留。VM reset 不重启系统时钟。不要在回调中 sleep 半小时或生成常驻 detached context；在 step 中比较下次刷新时间并迅速返回。

set 仅表示系统时间达到 2025-01-01 的合理下限，不证明最近同步、精确度或真实性。time 查询本身不触发网络。没有开放手动改系统时间或周期后台 SNTP。初次 HTTPS 请求可能因此等待一次校时。

本阶段不提供时区数据库、DST 转换或日期格式化；天气应用可请求所在地时区的时间字段并使用响应的时区信息。固定 UTC 偏移不能假定全年有效。

## 字符串

```lisp
(str-from-n 12.5 1)                       ;; "12.5"
(str-join (list (str-from-n 12.5 1) "C") " ") ;; "12.5 C"
```

- str-from-n 的可选精度是整数 0..6，不是 printf 格式字符串；默认整数完整十进制，实数最多 6 个有效数字；显式精度使用定点小数。拒绝非有限数或超过 95 字节的格式化输出。大整数显式转为小数格式会受 double 精度限制。
- str-join 接受 proper list、可选分隔符；最多 64 段、总输出 1024 字节（含分隔符），空 list 返回空字符串。拒绝非字符串、内嵌 NUL、不正规或循环/超长列表。
- 没有原样启用上游整个字符串扩展包：其中部分遍历/格式化接口不符合本项目的有界回调要求。只提供需要的兼容命名子集，不支持上游 str-from-n 的任意格式参数。
- 字符串可包含 UTF-8，不代表 LCD 有中文字库。gfx-text 仍为最多 128 个可打印 ASCII 字符、8×12 字形；大字号/中文/图片不在本阶段。

## 资源、构建与验证

HTTP 新增单个 8193 字节 PSRAM 正文缓冲、8192 字节内部任务栈及 SDK TLS/HTTP 的动态资源。JSON token/转换/字符串 workspace 放在原 runtime 的 PSRAM 存储中；扩展表容量 24→48，SD 启用时共注册 33 个扩展。HTTP 分配/任务创建失败保留普通 REPL，禁用时不编译/启动其 worker。不增加应用后台 Lisp context。

`:help-data` 为独立 19 行 ASCII 帮助页，普通终端可用；运行应用后仍用 :stop/:key q 退出，再查看帮助。主帮助仍满足 19 行限制。

主机模型覆盖：HTTP 完整性/限长/状态码/取消/超时与迟到结果、JSON Unicode/语法/边界/固定种子输入、真实 LispBM 转换/分配失败重试/应用中请求及退出、64 位时间、字符串边界、模拟 Open-Meteo 单日文档。fixture 是人为构造的示例，不是真实天气响应或用户位置。

完整结果与未测项见 [TEST_RESULTS.md](TEST_RESULTS.md)。真实 DNS/TLS/SNTP/Open-Meteo 响应、目标堆峰值/worker 栈水位/长期刷新、设备掉网/取消迟到与 LCD 画面均 **NOT TESTED**。开发镜像不是冻结候选，部署仍须新准确 hash/实际擦写范围批准和既有保护流程；不自动刷机或 commit/push。
