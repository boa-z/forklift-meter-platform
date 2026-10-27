# Product 接入

> [English](downstream.md)

## 仓库

每个 Product，无论公开还是私有，都是克隆到 `products/<product>`（或用绝对路径选择）的独立 Git 仓库。框架忽略 `products/*`，不记录 Product gitlink，也不需要私有 fork：所有 Product 都基于公开框架的 `main` 构建。客户代码、协议、UI、素材与需求都留在 Product 仓库。

```text
git clone PRODUCT_REPOSITORY products/my-product
cmake -S . -B build-my-product -G Ninja -DMETER_PRODUCT_ROOT=products/my-product
```

固件在板级 defconfig 里用 `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/my-product"` 指定 Product。没有任何机制固定 Product 提交，所以每次发布构建都要记录 Product、框架与 SDK 的提交。新 Product 用 `python tools/create_product.py --id my-product --output products/my-product` 起步。不要在 `contracts`、`core`、`runtime`、`protocols/common` 或 `ui/common` 里加入客户符号；缺少的通用能力向框架提出。

## Product 提供什么

`meter_product_get()` 返回一个静态的 `meter_product_t`（`contracts/meter_product.h`）。只有 `id`、`catalog`、`protocols` 与 `routes` 是必需的。

| 字段 | 用途 |
|---|---|
| `catalog`、`initial_settings` | 信号、本机设置、监视项、故障；首次开机的语言、速率、亮度、单位 |
| `protocols`、`routes` | 接收路由到无状态 DBC 解码或有状态适配器（SDO、CANopenNode 包装、私有握手、在 `process` 里轮询的串口） |
| `command_route` | 由哪个适配器执行 `meter_command_t`；App 经同步命令端口发送命令 |
| `periodic`、`periodic_count` | 按快照编码的周期帧（见[运行时](../runtime/runtime-production.zh-CN.md)） |
| `app_run`、`app_tick`、`app_reset` | 每轮运行时的 Product 业务；每个新会话开始时复位 |
| `local_action` | 来自 UI 或 MSH 的 `METER_ACTION_PRODUCT` 意图 |
| `ui` | LVGL 工厂，只在 UI 线程调用 |
| `storage`、`allow_parameter_extension` | 设置记录身份、去抖以及只增参数的格式扩展 |
| `update`、`update_admission`、`update_exclusive` | CAN 升级身份、容量、UDS 总线与标识、准入与维护行为 |

除 UI 工厂外，所有回调都在唯一的运行时线程里执行，所以 Product 状态不需要加锁。回调不得阻塞：会阻塞的外设（MCU 串口、摄像头、音频、振动）由 Product 自己开线程，通过 Product 自己的队列交换数据，再由适配器的 `process` 或 `app_tick` 取走。归一化的车型配置用 `meter_snapshot_profile()` 发布。

## 硬件差异

| 需求 | 位置 |
|---|---|
| 换 CAN 控制器、EEPROM 位置、用文件存设置、不独占显示层、自己控制背光 | 板级 defconfig，见[板级接入](../build/board-integration.zh-CN.md) |
| 分辨率、竖屏布局、触摸 | SDK 屏幕与 lvgl-aic 配置；Product UI 布局 |
| CANopenNode、私有协议、多条总线 | Product 在 `METER_BUS_CAN0`/`METER_BUS_CAN1` 上的适配器与路由 |
| MCU 串口、摄像头、振动、光感 | Product 代码与线程；视频要设 `AIC_FORKLIFT_DISPLAY_EXCLUSIVE=n`，自动亮度要设 `AIC_FORKLIFT_BACKLIGHT=n` |
| 不需要诊断或升级 | 不设 `update`，或 `AIC_FORKLIFT_CAN_UPDATE=n` |

## 按 Product 定容的域存储

平台不为你的目录声明容量。你自己声明数组并在初始化 core 时绑定：

```c
static meter_value_t signals[PRIVATE_SIGNAL_SLOTS];
static float parameters[PRIVATE_PARAMETER_SLOTS];
static meter_fault_state_t faults[PRIVATE_FAULT_SLOTS];
const meter_core_storage_t storage = {signals, PRIVATE_SIGNAL_SLOTS, parameters, PRIVATE_PARAMETER_SLOTS,
                                      faults, PRIVATE_FAULT_SLOTS};
meter_core_init(&core, &private_catalog, &storage);
```

任一表格大于绑定的存储时 `meter_core_init` 失败。固件组装需要三份这样的存储（运行时、发布副本、UI 副本）。标识是经目录解析的 16 位句柄，不是数组下标；私有扩展从 `METER_ID_PRIVATE_FIRST`（`0x1000`）开始。用 `meter_snapshot_read`、`meter_snapshot_parameter` 与 `meter_snapshot_fault_active` 读取。

设置数据块的大小由你自己的表格决定（`meter_settings_size()`）。版本与打包可以通过 `product/sources.json` 里两个可选键自动完成：`version.tool` 对 `--date YYMMDD --sequence N --format json` 打印 `{"ota": "...", "display": "..."}`，`identity.hardware` 给出升级包的硬件身份。见[用默认值代替变量](../build/build.zh-CN.md#defaults-instead-of-variables)。

### Product 语言资源

框架只提供翻译**服务**（`ui/common/i18n/meter_i18n_runtime.h`），措辞属于 Product。框架唯一内置的文字是英文：框架控件自己渲染的 `meter.*` 词汇（`meter.validity.live`、`meter.state.on`、`meter.value.error` 等）。Product 注册含同名标签的包即可覆盖其措辞。

```text
key  →  language catalog (static arrays, generated)  →  text
```

- **目录**：按 LVGL `lv_translation_add_static` 的版式生成静态包（语言码、以 NULL 结尾的键、键×语言）。运行时没有 JSON 或文件解析；来源（JSON、CSV、客户表）留在 Product 里，生成 C 数组。
- **注册**：在 `meter_firmware_composition_t.locale_init` 里对每种语言调用 `meter_i18n_register_language()`（持久化 id、包列名、可选的标签/数值字体），对每个包调用 `meter_i18n_register_pack()`。注册幂等，`meter_i18n_init()` 之后每个 LVGL 生命周期注册一次。
- **唯一来源**：持久化的 `snapshot.language` 决定语言。主循环在 `present_update()` 与 `present()` 之前调用 `meter_i18n_set_language()`，LVGL 随后向每个对象发送 `LV_EVENT_TRANSLATION_LANGUAGE_CHANGED`。
- **绑定而不是重建**：`meter_i18n_bind_text(label, tag, role)` 一次绑定翻译键与随语言变化的字体；`meter_i18n_bind_font()` 只绑字体。语言没有该角色的字体时，对象恢复设计时的字体。
- **回退**：`meter_i18n_tr()` 先取当前语言，再取已注册包的英文列，最后返回标签本身。生成完整矩阵，绑定的标签就不会显示键名。
- **模板与复数**：`{0}`–`{9}` 位置占位符由 `meter_i18n_format()` 按带类型的参数（`meter_i18n_arg_u/s/str`）展开，不经 printf。复数文字是基标签加 `.one` / `.other`，`meter_i18n_tr_plural()` 按当前语言的 `meter_plural_rule_t` 选形式。
- **多于两种语言**：`meter_language_t` 仍是持久化字节（`0` EN、`1` ZH）；Product 用 `meter_catalog_t.language_count` 声明目录接受多少个 id。
- **溢出文字**：给所有译文标签一个统一策略（例如 `LV_LABEL_LONG_SCROLL_CIRCULAR`），不要逐页改版式。

### Product 字体输入

在 **assets/font-config.json** 中用可选的 **text_sources** 列表声明动态 UI 文本目录。每项都是 Product 内的 UTF-8 源文件相对路径。**translations** 文件仍是主要输入；生成器合并两组输入并生成 14/20 px 字体子集。已声明文件缺失时生成和检查都会失败。运行 **python tools/generate_fonts.py --product-root products/my-product**，再加 **--check** 检查，生成的字体随 Product 一起提交。
