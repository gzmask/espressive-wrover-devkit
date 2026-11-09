# ESP-WROVER-KIT v4.1 官方参考资料

本目录发布乐鑫官方资料的来源、校验值与项目中文摘要。**原文件仅在本机保存，并由 Git 忽略，不随公开仓库分发**；克隆仓库后请从下方官方链接获取资料。本文不是官方译文，也不是新增功能的实机验收记录。

## 本地副本记录与官方来源

开发时已导入外壳项目保存的用户指南、正反面布局图和 DXF，原下载记录为 2026-11-07 UTC；原理图来自早期硬件核对保存的官方 PDF。五份本地副本未修改，导入时与来源逐字节一致，校验清单见 [SHA256SUMS](SHA256SUMS)。在线文档可能更新，重新下载后不保证仍匹配历史校验值。

| 本地副本名称（不发布） | 官方来源 | 用途 |
| --- | --- | --- |
| `user_guide.html` | https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp-wrover-kit/user_guide.html | 功能介绍、跳线、引脚分配、官方示例入口 |
| `layout-front.png` | https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/_images/esp-wrover-kit-v4.1-layout-front.png | USB、供电、按键、排针等位置 |
| `layout-back.png` | https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/_images/esp-wrover-kit-v4.1-layout-back.png | LCD 面布局，不是用户实物照片 |
| `ESP-WROVER-KIT_V4.1.dxf` | https://dl.espressif.com/dl/schematics/ESP-WROVER-KIT_V4.1.dxf | 板框与安装孔名义几何，不是完整 3D 包络 |
| `ESP-WROVER-KIT_V4_1.pdf` | https://dl.espressif.com/dl/schematics/ESP-WROVER-KIT_V4_1.pdf | 电气连接；标题 ESP32-WROVER-KIT_V4.1，日期 2018-08-29 |

### 离线阅读边界

- `user_guide.html` 是原始网页快照，**不是完整离线站点**。网页中的相对图片、CSS、其他页面以及部分在线脚本未一并保存；板卡正反面图片可直接打开本目录 PNG。
- `latest` 是原下载时的内容，不保证与今天在线页面一致。HTML 的源码编辑链接标识文档提交 `5c06bf1`。
- 指南当前文字使用 ESP32-WROVER-E 模组名称，不能据此认定本机模组标签；本机已实测芯片/Flash/PSRAM 与尚未核对的模组型号以 [硬件盘点](../../HARDWARE.md) 为准。
- 网页的其他资料与示例链接未下载，不表示已安装 BSP 或实现 SD/摄像头。OpenSCAD 下载缓存、外壳模型、派生测量和用户实物照片均未导入。

## 能用来做哪些项目

| 官方硬件能力 | 可考虑的玩法 | 本仓库当前状态 / 注意事项 |
| --- | --- | --- |
| SPI LCD | 天气钟、服务状态屏、像素动画 | ILI9341 已运行；这些应用界面尚未实现，无触控验收证据 |
| microSD 卡槽 | 图片素材、离线相册、传感器日志 | 驱动未实现；与 LCD、JTAG、RGB LED 存在共享引脚 |
| RGB LED | 状态灯、简单 PWM 灯效 | GPIO0/2/4，驱动未实现；不能忽略 BOOT/SD/摄像头共享 |
| JP1 I/O 排针 | 外接 I2C 传感器、按键、其他低速外设 | 需先排除已用引脚、启动绑带及电气限制，不默认排针全部空闲 |
| JP4 摄像头接口 | 外接摄像头实验 | 官方描述标准 OV7670 接口；不代表板上已有摄像头，也不默认兼容任意模组 |
| FT2232HL USB 桥 | UART 日志、JTAG 调试 | A 通道 JTAG，B 通道 UART；UART 已用，JTAG/跳线尚未验收 |

**优先做不需改接线的 LCD＋Wi-Fi 应用。** SD、摄像头、传感器与灯效均是候选后续工作，不因存在官方接口就视作已支持。

## 引脚与并用限制

以下依据指南的 `Allocation of ESP32 Pins` 及各外设表格：

| 外设 | GPIO / 信号 | 关键限制 |
| --- | --- | --- |
| PSRAM | CS=16，CLK=17 | 本机正在使用，不能当作自由 GPIO |
| 内部 Flash | 6–11 | 内部 Flash 总线，不用于普通外接 SPI 设备 |
| LCD | RESET=18，SCLK=19，D/C=21，CS=22，MOSI=23，MISO=25，背光=5 | 本机固定配置正在使用；多项与摄像头共享 |
| SD | CLK=14，CMD=15，DATA0=2，DATA1=4，DATA2=12，DATA3=13，Card Detect=21 | GPIO12–15 与 JTAG 共享；GPIO21 与 LCD D/C 共享 |
| RGB LED | R=0，G=2，B=4 | GPIO0 与 BOOT 共享，GPIO2/4 与 SD 共享 |
| 摄像头 | 0/4/5/18/19/21/22/23/25/26/27/34/35/36/39 | 大量与 LCD 共享，不能默认原接线可同时独立工作 |
| 32.768 kHz 晶振 | 32/33 | 指南说明默认接晶振、未接 JP1；不能默认拿来接 I2C |

- **LCD＋SD：** 指南指出 GPIO21 同时连接 LCD D/C 与 SD 卡检测；给出的硬件处理方式是移除 R167 以断开卡检测。仅在软件中忽略卡检测不能假定电气冲突已消失。本项目没有做该改板操作，也没有批准拆焊；开发前须另行确认可行接法。
- **SD＋JTAG：** 多个引脚共享，不能直接按两套默认配置同时启用。具体 SD 总线模式和 JTAG 跳线需单独规划。
- GPIO0/2/12/15 等涉及 ESP32 启动绑带，外接器件不能任意改变启动时电平；GPIO34–39 是仅输入引脚。具体电气条件需查 ESP32 数据手册和原理图。
- EN 是复位按键，不是普通游戏输入；BOOT 与 GPIO0/启动模式相关，也不是独立专用用户键。

## 供电与机械资料边界

- 指南支持 USB 或外部 5 V，经 JP7 选择供电。桶形插座标称 5.5 × 2.1 mm、中心正极；这不是插头外壳或外壳开孔尺寸。
- 本次不改变跳线、供电、电压、GPIO 接线或器件；指南中的改板选项不构成操作授权。
- DXF 为 AC1009/R12，单位码未声明；毫米依据来自图中明确尺寸标注。名义板框约 85.146 × 84.327 mm，带台阶，不能简单替换为正方形。几何解析与试配结论保留在外壳项目，不作为本机机械适配证明。
- 新固件仍使用固定 ESP-IDF v5.5.6 及现有健康确认/回退配置，不能直接用指南里的任意示例覆盖当前应用；刷写须重新批准准确 hash、目标槽及范围。

## 官方软件入口（仅参考）

- BSP：https://components.espressif.com/component/espressif/esp_wrover_kit
- LCD 示例（指南引用 v5.3）：https://github.com/espressif/esp-idf/tree/v5.3/examples/peripherals/spi_master/lcd
- SD 示例（指南引用 v5.3）：https://github.com/espressif/esp-idf/tree/v5.3/examples/storage/sd_card
- 摄像头驱动：https://github.com/espressif/esp32-camera
- ESP32 数据手册：https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf

这些链接不是本项目依赖版本选择，也不是实机验证结果。当前实现和授权边界见 [项目 README](../../../README.md)、[硬件盘点](../../HARDWARE.md) 与 [OTA](../../OTA.md)。

## 完整性与版权

取得五份原文件并保存到本目录后，可运行 `shasum -a 256 -c SHA256SUMS` 核对历史副本；公开克隆默认不含原文件，直接运行会报告文件缺失。SHA-256 不替代官方签名或来源认证。

制造商文件与图片版权归原权利人；本地参考不代表已取得单独再分发许可。上游文档仓库含 Apache-2.0 许可，但不能据此推定外部下载的 PDF/DXF 及全部图片均已获准单独再分发。**公开仓库仅保留来源链接、校验值与项目摘要，原文件留在本地。** 不修改 `.local/` 中的冻结固件和验收归档。
