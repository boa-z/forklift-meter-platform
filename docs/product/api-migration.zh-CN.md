# Product 迁移到 Luban-Lite HMI

> [English](api-migration.md)

项目 `forklift-meter-platform` 更名为 **Luban-Lite HMI Framework**（仓库 `boa-z/luban-lite-hmi`）。更名只改标识符，行为、持久化数据和线上格式不变。针对旧 Framework 编译的 Product 需要做下面的修改。Product 是独立仓库，不会被自动更新。

## 名称

| 旧 | 新 | 说明 |
|---|---|---|
| `application/rt-thread/forklift-meter-platform` | `application/rt-thread/luban-lite-hmi` | SDK 应用目录；`CONFIG_PRJ_APP="luban-lite-hmi"` |
| `meter_*`、`METER_*`（C API、CMake 函数/目标、Python 工具） | `hmi_*`、`HMI_*` | Framework 定义的全部符号、头文件与源文件，例如 `contracts/meter_product.h` → `contracts/hmi_product.h`、`meter_product_t` → `hmi_product_t` |
| `CONFIG_AIC_FORKLIFT_METER_PLATFORM_APP` | `CONFIG_AIC_HMI_APP` | Kconfig 应用开关 |
| `CONFIG_AIC_FORKLIFT_*` | `CONFIG_AIC_HMI_*` | 板级资源、升级、看门狗、显示选项 |
| `-DMETER_PRODUCT_ROOT`、`-DMETER_BUILD_UI`、`-DMETER_ENABLE_UPDATE`、`-DMETER_LVGL_ROOT` | `-DHMI_…` | 主机 CMake 选项 |
| 环境变量 `METER_PRODUCT_ROOT`、`METER_CAN_UPDATE`、`METER_UPDATE_VERSION`、`METER_DISPLAY_VERSION`、`METER_VERSION_SEQUENCE`、`METER_BOARD_ID`、`METER_OTA_PACKAGE`、`METER_PLATFORM_ROOT` | `HMI_…` | 构建与测试环境 |
| `meter-demo`、`meter-protocol-runner` | `hmi-sim`、`hmi-protocol-runner` | 主机可执行文件 |
| 生成目录 `output/<project>/meter/`（`meter_build_identity.h`、`meter_update_build.h`、`meter_display_version.h`） | `output/<project>/hmi/`（`hmi_…h`） | 自动当日序号 `V<n>` 在新目录里重新计数 |
| MSH `meter info\|diag\|…`、`meter_color`、`meter_display`、`meter_testcard`、`meter_settings`、`meter_exec`、`meter_watchdog`、`meter_update` | `hmi …`、`hmi_color`、…、`hmi_update` | `meter_update` 保留为 `hmi_update` 的别名（见下） |
| 日志标签 `meter.*`、线程名 `meter_app`、`meter_tx0` 等 | `hmi.*`、`hmi_app`、`hmi_tx0` 等 | 串口日志过滤条件需同步 |

## 刻意保持不变的部分

- 持久化数据：设置记录（`MSP3`）、NVM 槽格式（`FMP2`）、Product 命名空间与 schema，以及默认设置文件前缀 `/data/meter-settings`（使用文件后端的板子保留已存设置）。
- UDS/ISO-TP 升级协议、DID、OTA 包结构以及 Flash/分区用法。
- 独立工具 `boa-z/meter-ota-host`（`tools/ota`）：可执行文件名 `meter-ota-inspect`、变量 `METER_OTA_INSPECTOR`，以及它默认的维护命令 `meter_update maintenance on`，固件通过别名继续接受。
- Demo 仓库名 `boa-z/forklift-meter-platform-demo`，以及 Product 自有名称（例如 Demo 的 `meter_gauge_*` 控件）。

## 更新 Product

替换 Product 源码、CMake、脚本和文档中的 Framework 标识符；Product 自有的 `meter_*` 名称保持不变。按记号精确替换（只替换 Framework 定义的标识符）是安全的；盲目把 `meter_` 换成 `hmi_` 会连 Product 的名字一起改掉。之后用 Framework 工具重新生成 i18n/CAN 文件并运行主机测试。
