# 蓝牙键盘接入调研（离线，尚未实现）

后续用户已选择单槽＋USB固件更新/恢复，准确批准后0.3.0已各一次USB应用/表迁移、180.203秒正常启动只读验收通过，见 [SINGLE_APP_USB.md](SINGLE_APP_USB.md)。当前为factory2.625MiB，余约1.69MiB；下文263KiB仅旧双槽预算。蓝牙栈/内部RAM仍未测，配对/NVS阻断没有因此消失，旧控制区reserved也不能擅自用于新存储。

以下为最初仅源码调研的证据；该阶段没有设备扫描/配对/输入或写入。当时设备0.2.4 B/VALID，后来单槽迁移是另行准确批准的独立阶段。蓝牙键盘至今仍 **NOT IMPLEMENTED / NOT TESTED**，未因固件迁移而测试或启用，当前全部写权限耗尽。

## 结论与待确认项

ESP-WROVER-KIT v4.1 的经典 ESP32支持 BR/EDR 和 BLE；硬件不需新增 GPIO、接线或 USB Host。项目需要的是 **HID Host**（接收键盘），不是让 ESP32 模拟键盘的 HID Device 库，也不是 BLE UART。

先针对一把实际键盘实现，不预先开发双模通用驱动。需用户提供：

1. 键盘品牌、准确型号，以及当前选择的 Bluetooth 模式。带 2.4GHz USB 接收器不等于支持蓝牙；“Bluetooth 5.x”也不足以确定 HID 传输类型。
2. 键盘布局：首版建议 US ASCII，满足 Lisp 所需大小写、数字、括号、引号和标点；不做中文输入法。
3. 是否接受设备重启后重新配对。现有边界禁止改变旧 NVS，因此不能默认实现永久记住键盘；键盘端保留旧 bond 时也可能需要重新进入配对模式或清除该主机记录，须按型号确认。

| 键盘类型 | 可用 SDK 路径 | 本项目的选型判断 |
| --- | --- | --- |
| 经典蓝牙 HID / BR/EDR | Bluedroid + `esp_hid` / Classic HID Host | 支持协议，但经典 bond/config 的 NVS 路径是待解决阻断；不能直接上板 |
| BLE HOGP | NimBLE GAP/GATT，或 Bluedroid BLE + `esp_hid` | 优先研究 NimBLE 的 RAM-only 最小 HOGP client；现成 NimBLE HID 封装有配对限制，不能直接采用 |
| BLE＋经典双模 | Bluedroid dual mode | 芯片和 SDK 支持；首版不必同时开两种模式，先按实际键盘选一种 |
| 只有专有 2.4GHz USB 接收器 | 不属于蓝牙 HID/HOGP | 本轮方案不适用；经典 ESP32 没有原生 USB OTG Host，不扩展为 USB Host 项目 |

后续音箱若采用 A2DP 会需要经典蓝牙能力，但这不等于现在必须启用音频或双模；NimBLE BLE-only 方案不能直接提供 A2DP，届时须重新评估栈与资源。

## 已核查的 SDK 证据

以下相对路径均位于 `.local/esp-idf/`，commit `880a203ae603d08a94d06aa01e0d6761958e5e9e`；未改上游文件。

### HID 接口与示例

- `examples/bluetooth/esp_hid_host/` 同时包含 Bluedroid 和 NimBLE 路径。默认配置启用 Bluedroid dual mode、Classic HID Host 与 BLE；示例另选 large single-app 分区，**不能照搬其分区或 flash 命令**。
- `components/esp_hid/include/esp_hidh.h`：`esp_hidh_init()`、OPEN/INPUT/CLOSE 事件、raw report map 查询、output report API。INPUT包含 `usage`、`map_index`、`report_id`、`length` 和原始 `data`，不是 ASCII 文本。
- `components/esp_hid/src/esp_hid_common.c`：识别 keyboard collection、报告类型和总长度；不会自动将任意键盘 report 翻译成字符，也不能只凭 `usage=KEYBOARD` 或8字节长度就认定 boot layout。
- `examples/bluetooth/esp_hid_host/main/esp_hid_host_main.c`：示例按扫描结果/名称选择设备并打印每份 report。项目不能自动连最后一个发现的设备，也不应逐键 dump，以免再次刷走 IP/端口提示。
- API 说明要求处理 CLOSE 时调用 `esp_hidh_dev_free()`；接入时必须核查所选 backend 的实际释放顺序。跨任务只传有界值拷贝和会话代号，不保留回调 `data` 或可能失效的设备指针。

### 配对不是可以原样照抄的部分

- `examples/.../main/esp_hid_gap.c`：经典示例自动确认 numeric comparison、legacy PIN使用 `1234` 或16个0；BLE示例也自动确认，并有未完成的 passkey input处理。不能把这些示例行为当作实际键盘兼容性或合适的配对实现。
- 更重要的是 `components/esp_hid/src/nimble_hidh.c` 本身：PASSKEY NUMCMP自动接受、INPUT固定 `123456`、DISPLAY仅打印请求；初始化还设 `sm_io_cap=KEYBOARD_ONLY`。其公开封装未提供专用配对事件回调，现成示例外层修改不足以解决这一点。
- 对只有输入能力的常见键盘，主机可显示本次配对码，用户在键盘输入后按 Enter；实际采用哪种配对方法仍由双方能力/协议决定。配对中的按键不能作为 Lisp 源码输入。
- 不修改固定 SDK 去打补丁。若实际键盘是 BLE，下一离线验证应优先尝试原生 NimBLE GAP/GATT，用可控的配对回调实现一台设备的 HOGP client；不是直接宣称现成 `esp_hid` 已可用。

### NVS 边界

- 官方 HID示例 `app_main()` 先 `nvs_flash_init()`，遇到旧版本/空间不足还会 `nvs_flash_erase()`。本项目禁止复制这些行为。
- NimBLE：`components/bt/host/nimble/Kconfig.in` 的 `BT_NIMBLE_NVS_PERSIST` 默认n；`components/bt/host/nimble/port/include/esp_nimble_cfg.h` 将其映射为 `BLE_STORE_CONFIG_PERSIST=0`。store/config提供RAM记录，持久化实现受该宏保护；这是值得验证的 RAM-only 路径，不是已实测无写入保证。
- Bluedroid BLE：有 `BT_BLE_SMP_BOND_NVS_FLASH`（默认y）和 `BT_GATTC_CACHE_NVS_FLASH`（默认n）。关闭这两个选项**不等于整个 Bluedroid 不访问 NVS**。
- `components/bt/host/bluedroid/btc/core/btc_config.c` 的初始化调用 `config_new()`/`config_save()`；`components/bt/common/osi/config.c` 使用 `nvs_open(..., NVS_READWRITE)`/`nvs_commit()`。Classic `btc_storage.c` 还有 bond/config flush；未找到对应的经典全局“只用RAM”配置开关。
- 因此经典蓝牙路径在当前“不改上游、不动旧 NVS”约束下仍待解决。仅不初始化NVS、让调用失败，不能未经验证就当作可靠支持方案；不擅自新增存储分区、链接替换 SDK 私有函数或放宽保护范围。
- 不持久化不等于取消链路加密/配对；只是不把键盘密钥写到 Flash。未来若要求永久记忆键盘，须单独讨论存储策略与准确写入范围，不能视为普通应用 OTA 的隐含权限。

## 最小输入接入方案（设计，尚未编码）

```text
Bluetooth GAP/GATT 或 HID Host
          ↓ 有界 report/状态值拷贝（不求值、不刷LCD）
     单键盘队列＋会话代号
          ↓ 原 REPL owner 内处理
  report解码 → US按键映射 → 输入来源仲裁 → repl_frontend_feed()
                                                    ↓
                                               原 LispBM
                                                    ↓
                                         原 LCD owner/SPI DMA
```

当前 `components/lisp_repl/lisp_repl.c` 的 `input_byte()` 是static、只从UART owner循环调用；TCP通过完整源码邮箱进入同一个owner。不能在蓝牙任务里直接调用frontend、LispBM或LCD，也不能简单增加第二个任务共用static frontend。

首版建议范围：

- 一把键盘、一个连接、一次显式配对窗口；确认目标后才连接，不建永久设备数据库，不按最强RSSI/最后结果自动选键盘。名称和MAC只能帮助选设备，不是密码学身份保证；BLE随机地址不能永久硬编码为身份。
- 先支持经过描述符/协议确认的 boot-compatible 6-key报告，或这把键盘的准确 report格式；长度、report ID及是否含reserved byte要按backend核实。NKRO、厂商格式和媒体键不猜测解析，未支持时明确停止该输入。
- 通过前后按键集合差分处理 key-down/up；重复报告不重复打字。支持左右Shift/Ctrl、Caps Lock状态、Enter、Backspace/Delete、Tab→空格和 US标点。Caps Lock只影响字母，与Shift做组合；首版不要求锁定灯 output报告。
- Ctrl-C沿用本地UART语义：取消并清全部RAM定义；Ctrl-U清当前行。方向键/历史/编辑器不新增。Ctrl/Alt/GUI组合未经支持不能退化为普通可打印字母；不让Ctrl-C/Enter参与自动重复。
- 可先不做自动连发；若增加长按重复，由owner有界计时，仅针对允许的普通键/退格，松键、掉线或队列损坏立即停止。
- UART与键盘必须按“未完成的一次提交”区分来源，不能拼接两路半句。TCP继续在本地输入未完成/执行中返回busy；未获得来源所有权的键盘输入丢弃并有限提示，不能积压到别人提交结束后偷偷执行。
- 断开清除该键盘来源的未提交输入、按键状态和待处理事件，不自动Enter、不重放提交、不触发RAM reset。队列丢失/非法report则隔离本次输入并要求重新建立完整状态，不能悄悄漏字后继续求值。重连用新会话代号，旧回调不能影响新输入。
- 默认关闭蓝牙功能。连接成功/失败/配对码由owner在适当时机简短显示，不逐键日志、不自动eval、不追加整段启动help；状态通知不得插进用户求值结果。

## Wi-Fi / OTA / 资源

- ESP32只有一个2.4GHz RF，Wi-Fi与BT/BLE分时使用。固定SDK共存文档列出STA与BLE scan/connected、经典inquiry/page/connected均受支持；这不是本项目的延迟、掉键或OTA稳定性验收。
- 同时启用Wi-Fi/BT时核查 `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`；现有配置的 `ESP_COEX_ENABLED` 不能代替该检查。官方建议Wi-Fi与BT Controller/Host分配到不同CPU；先保持LCD/REPL所有权不变，不凭猜测缩减Wi-Fi/TLS缓冲或启用GPIO调试。
- 当前 `partitions.ota.csv` 每个app槽 **0x150000 = 1,376,256字节**。0.2.4镜像1,106,720字节，文件大小余量 **269,536字节（约263.2KiB）**，尚未扣除新镜像自身对齐变化。需要测新增代码、静态DRAM/IRAM和运行期内部堆；不能凭PSRAM大就认为够用。
- 下一步只在新独立目录编译单一协议的最小原型与原OTA/TCP组合，使用原固定SDK/上游，不覆盖部署或冻结目录；记录 map/size，不改分区。如果超槽或资源不足，先报告阻断，不删除OTA保护或自动迁移Flash。
- 原PSRAM/LCD/心跳/Wi-Fi/HTTPS/REPL健康与回退门控保留。键盘关机或尚未配对不是固件不健康，不以“必须连上键盘”作为OTA确认条件；新增任务不能饿死原owner/健康监测。初始化错误处理和资源隔离需另行设计/测试，不能用反复重启掩盖故障。

## 后续验证顺序

1. **型号确认**：确定BR/EDR或BLE、配对步骤、布局及重启后重新配对是否可接受。
2. **离线选型验证**：NVS调用路径与配对回调先过关；再编译量化单栈＋现有OTA/TCP资源。此步不创建已批准候选、不访问板子。
3. **纯逻辑测试先行**：report/ID/长度、按键差分/重复/rollover、Shift/Caps/标点/Ctrl、掉线/队列溢出/过期会话、UART/TCP来源冲突。若支持更多descriptor，补有界解析与畸形输入测试。
4. **可部署候选**：保留原密钥、回退及启动probe，冻结新的准确hash/大小/擦写范围；若届时仍B运行，下一目标A会覆盖0.2.3备用。只有新批准后才按当前基线和一次OTA流程操作。
5. **另获交互许可后的有限实机验收**：扫描/配对/实际打字、大小写及Lisp标点、取消/多行、掉线松键状态、重连、输入冲突、Wi-Fi/HTTPS/LCD/owner健康。镜像批准不等于允许自动UART/TCP测试表达式。

本轮没有蓝牙代码变更、蓝牙构建或硬件测试。新独立主机构建目录 `.local/host-build-bt-research` 和 `.local/host-build-bt-research-word32` 的既有 C ASan/UBSan 回归各 **92/92通过**；另 **15 Python / 4 OTA uploader mock通过**，没有真实设备调用。构建保留既有cJSON deprecated及刻意value32/pointer64模型的上游转换警告，未修改/屏蔽。这些只说明既有代码回归，不构成蓝牙能力或不写NVS的证据。

## 官方参考

- [v5.5.6 Bluetooth API：Bluedroid与NimBLE](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/bluetooth/index.html)
- [v5.5.6 Classic HID Host API](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-reference/bluetooth/esp_hidh.html)：注意页面介绍的低层 `esp_hidh_api.h` 与组件统一封装 `esp_hidh.h` 不是同一层接口。
- [固定commit HID Host示例](https://github.com/espressif/esp-idf/tree/880a203ae603d08a94d06aa01e0d6761958e5e9e/examples/bluetooth/esp_hid_host)
- [固定commit esp_hid组件](https://github.com/espressif/esp-idf/tree/880a203ae603d08a94d06aa01e0d6761958e5e9e/components/esp_hid)
- [v5.5.6 RF共存](https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32/api-guides/coexist.html)

最终实施以这把键盘的实际报告/配对行为和固定SDK源码为准，不照搬搜索结果中的Arduino HID Device库或`stable/latest`版本行为。
