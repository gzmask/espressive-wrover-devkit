# Open-Meteo 天气屏（离线源码，未部署）

`examples/lisp/weather.lisp` 是一个 RAM-only LispBM 应用：面向早晨出门前快速查看，不引入固件专用天气服务、图片解码或后台 Lisp context。

**当前范围**：在用户明确同意后，以 HEAD `a17b5d9f6fccfcffff7d49cc101f95d2c93d8534` 加上一轮未提交数据 API 的本地工作树为基线开发；本轮不声称这个 dirty tree 已重新与上游同步。保留已有改动，仅新增天气 Lisp/测试/fixture 和文档。实现阶段没有真实天气 API 请求、设备/实卡访问、复位、刷机或冻结；源码提交与推送不构成固件部署或设备操作许可。官方文档读取不是天气数据验收。

板上最后记录仍是 0.5.12，**没有新 HTTP/JSON/时间/字符串 API，不能在该版本直接加载本应用**。部署须另行冻结新固件、批准准确 hash/擦写范围并完成保护；不能因源码或主机测试通过自动操作设备。

## 页面

320×240 一页，不轮播：

- 当前温度为七段式大数字，旁边是天气状态和体感温度。
- 今日高低温；当地 API 时间的 07:00、12:00、18:00 温度与降雨概率。
- 只根据当前模型时间之后的降雨概率提示是否带伞，阈值为 50%；不把早上已经过去的雨当成下午降雨。
- 底部显示模型数据时间、数据年龄或错误/过期原因，以及来源与按键提示。
- 文字只用现有 ASCII 字体，温度单位显示 C/F。未知可选值显示 --/UNKNOWN，不用 0、晴天或低降雨代替缺失。
- 本应用显示的是**天气模型预报**，不是本地传感器或独立实时气象站观测；带伞提示不是官方安全警报或 AI 建议。

主机 fixture 的正常摄氏/华氏画布均与独立 C 参考逐像素比对。私有 `.local/weather-app-dev1/forecast-{c,f}1.bmp` 是离线模拟画布预览，不是 LCD 拍照、颜色/可读性或物理 FPS 验收。

## 配置与运行（供未来已部署新 API 的版本使用）

加载源码只定义函数；不会自动运行或发天气 GET。没有默认位置，也没有 IP 定位、城市搜索或后台定位。

```lisp
;; 下列 0,0 仅说明调用格式，不是用户位置；请替换为自己的经纬度。
(weather-config 0 0 'celsius)
;; 或 (weather-config LATITUDE LONGITUDE 'fahrenheit)
;; 返回 t 才代表配置接受；nil 表示无效且原配置未修改。

(app-run weather)
```

纬度须在 [-90,90]，经度在 [-180,180]，单位只接受 celsius/fahrenheit。原始配置驻留 RAM；GET 使用四位小数，页眉显示三位小数。固定请求摄氏，华氏在应用内以 double 换算并四舍五入；数字匹配先将 JSON 的 int64 天气码归一到小整数，避免跨类型 eq 不匹配。

未配置也可运行页面查看配置提示，**不会发 GET**。先 q 退出，再配置并重新 app-run；应用期间普通 REPL 求值仍被原 app gate 拒绝。

- UART q / TCP `:key q`：正常退出并恢复终端。
- UART r / TCP `:key r`：手动刷新；本次运行内距离上次尝试至少 60 秒，pending 时不另开请求。
- 原 UART Ctrl-C / TCP `:stop` 仍可停止应用。
- 正常退出保留配置、函数和最后快照供检查；再次运行会清旧快照并重新取数。VM reset/重启会丢失全部定义。
- 源码约 16KiB，超过普通 REPL 的 1023 字节限额；未来可用源码 URL/SD loader，不要直接长粘贴。SD 仍遵守关 LCD、插卡加载、卸载取卡再开 LCD 的硬件流程；本轮未执行这些步骤。
- 没有安装、持久缓存、开机自动加载/运行或断电离线显示，这些是后续独立工作。

## 获取数据与生命周期

一次 GET 请求 Open-Meteo Forecast：

```text
current=temperature_2m,apparent_temperature,weather_code
hourly=temperature_2m,precipitation_probability
daily=temperature_2m_min,temperature_2m_max
temperature_unit=celsius
timezone=auto
timeformat=unixtime
forecast_days=1
```

经纬度由配置加入。使用上一轮通用 [Lisp 数据 API](LISP_DATA_APIS.md)，不改固件实现：

1. init 发起一个异步 GET，立即绘制等待页面。
2. step 轮询，terminal result 只取一次；复制后立刻 release HTTP 槽。
3. JSON 验证后只保留一个紧凑快照（当前值、三组小时值、剩余降雨上限、日期/时间等），不长期持有原始正文或完整 JSON 树。
4. 每 30 分钟从尝试起点安排新请求，不累计漏掉的刷新债务。失败也是等下一周期，或由用户在 60 秒节流后按 r；这不是传输层盲重发。
5. pending 超过 35 秒，release/abandon 并显示 timeout。若 SDK worker 尚未退出，后续请求可以 busy；不提前复用缓冲或创建替代 worker。
6. q/stop/reset 按既有所有权规则释放/取消自己的请求；迟到结果不能覆盖快照。
7. 仅状态、数据年龄分钟或过期标志变化才重绘。底层既有 LCD owner 的提交周期和 app tick/画布复制策略不变，**不是低功耗或停止 SPI 的实现**。

首次失败时显示无预报及错误；后续更新失败保留最后成功快照，但立刻标红 STALE。无网络不导致应用退出；真实 OOM/坏回调仍遵守 VM 的失败/恢复规则。

## 数据完整性与日期

缺失/坏 JSON、错误单位、错误类型、非法范围或时间不匹配，均不替换旧快照：

- 当前温度与每日高低温必须为合理摄氏数值（-100..70），low<=high；体感和 WMO 可缺失。有效但未知 WMO 码显示 UNKNOWN。
- 当前、每日、逐小时的时间单位须为 unixtime；温度元数据为摄氏，降雨单位为 %。
- 当地 API 日界须为午夜；三组小时数组必须各有 24 项，时间从日界开始逐小时连续，不能猜数组位置。温度/概率 null 可显示未知；概率必须为 0..100。任一剩余小时概率未知或没有剩余小时，提示 RAIN DATA MISSING。
- 系统 UTC 必须已设；模型当前时间最多落后系统 2 小时或超前 15 分钟，且模型、每日预报和系统必须属于响应 offset 下的同一日期。旧日/未来/过老响应被拒绝。
- 快照年龄由接收时模型延迟加单调经过时间计算；达到 60 分钟标 STALE，即使刚拿到的响应本身已旧。系统时钟回拨不把快照变新。
- 跨日、时钟丢失或相对模型明显回拨，显示 STALE: clock/date mismatch；日期仍保留旧快照的明确年月日，不再称作今天，也不继续给出正常带伞建议。
- 采用响应的 utc_offset_seconds 与 daily.time，不从机器时区或 IP 猜位置。日期格式化在 Lisp 内完成，覆盖闰日与年份变化。

Open-Meteo 官方文档说明 offset 在请求时解析，响应范围内的 DST 变化不逐条处理。本应用沿用该**单次响应的固定 offset 时间轴**，每次刷新重新读取 offset；**没有 IANA 时区数据库，不宣称完整 DST 转换**。对不满足 24 项/连续小时结构的响应直接拒绝。见 [Forecast API 文档](https://open-meteo.com/en/docs)。

## 许可、隐私与边界

面向个人非商业天气屏，使用 Open-Meteo 的免费接口，无 API Key。页面来源为 Open-Meteo.com；数据来源/许可见 [Open-Meteo](https://open-meteo.com/) 与 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)。本应用将数据筛选、换算、舍入并按阈值生成提示；商业托管使用须重新核对 [服务计划与条款](https://open-meteo.com/en/pricing)。

配置坐标会出现在请求 URL 并发送给服务商；REPL 仍可能回显提交的配置。仅在可信 LAN 使用现有无认证 TCP，不在源码提交自己的位置或其他凭据。示例/fixture 的 0,0 和 2030 年日期均为合成数据，不是用户或真实天气记录。

## 验证

25 个新主机用例：仅加载/无位置不联网、无效配置、日历/天气码、摄氏/华氏完整像素、负温/舍入、30 分钟调度/手动节流、网络失败保留旧值、13 类 schema/时间/单位错误、null、35 秒超时/退出取消、过期/午夜/时钟丢失与回拨、整小时/半小时时区、未启用 HTTP 后恢复、未返回 worker 的 busy/迟到隔离、8 次启停、32 次半小时模拟更新（含跨日）、只统计剩余降雨。

两主机模型各 253 CTest、28 Python、4 历史 mock 通过，保留已知上游 UBSan 对齐诊断；不是 sanitizer-clean 或 FreeRTOS 实机。新 full 开发构建复核通过，但 Lisp 应用未编入固件、未冻结或部署。真实 Open-Meteo/DNS/TLS/SNTP、目标堆峰值/解释器压力/worker 栈、长期运行、LCD 目视/触控/物理 FPS 均 NOT TESTED。证据目录 `.local/weather-app-dev1/`，具体见 [TEST_RESULTS.md](TEST_RESULTS.md)。
