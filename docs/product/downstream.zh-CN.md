# 私有下游指南

> [English](downstream.md)

从已打标签的公共平台版本启动新的私有仓库，并设置：

```text
origin   → private Framework fork
upstream → https://github.com/boa-z/forklift-meter-platform
```

fork 的 `main` 保持与 upstream 同步；开发分支通过 `products/<product>` 固定私有 Product 仓库。客户代码、协议、UI、素材、需求和验证文档均放在独立 Product 仓库内部。Framework fork 只保存 gitlink 和通用集成，固件通过 fork defconfig 中的 `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/<product>"` 选择其 `product/sources.json`（宿主 CMake 使用 `-DMETER_PRODUCT_ROOT`）。客户数据及 Product 历史不得进入公共 Framework。不要向 `contracts`、`core`、`runtime`、`protocols/common` 或 `ui/common` 添加客户符号和条件；缺少通用能力时先在上游完善。

Product 可以通过 `product/sources.json` 中两个可选键让版本号和打包自动化：`version.tool` 指向 Product 内的 Python 3 脚本，接受 `--date YYMMDD --sequence N --format json` 并输出 `{"ota": "...", "display": "..."}`（分别最多 31 和 47 个字符）；`identity.hardware` 给出升级包使用的硬件标识（缺省取 `product/product.c` 的 `.hardware`，再缺省为 `reference-board`）。日期和当日序号由 Framework 决定，Product 只决定字符串的样子。见[用默认值代替变量](../build/build.zh-CN.md#用默认值代替变量)。

## 产品规模的域存储

平台不为你的目录声明容量。你在初始化核心时声明数组并绑定，因此 128 信号的产品和 14 信号的 Demo 各自只占用自身所需的内存：

```c
static meter_value_t signals[PRIVATE_SIGNAL_SLOTS];
static float parameters[PRIVATE_PARAMETER_SLOTS];
static meter_fault_state_t faults[PRIVATE_FAULT_SLOTS];
const meter_core_storage_t storage = {signals, PRIVATE_SIGNAL_SLOTS, parameters, PRIVATE_PARAMETER_SLOTS,
                                      faults, PRIVATE_FAULT_SLOTS};
meter_core_init(&core, &private_catalog, &storage);
```

`meter_core_init` 在任一表格大于所绑定的存储时失败，因此过小的产品配置表现为启动错误而非内存损坏。`products/demo/product/demo_storage.c` 是 Demo 规模下的相同模式。静态存储已足够；`contracts`、`core`、`runtime`、`protocols` 或 `products` 中没有任何部分使用堆。

信号、参数和故障标识均为通过你的目录解析的 16 位句柄，而非数组下标。公共 Demo 条目使用 `1..0x0FFF`；私有扩展范围起始于 `METER_ID_PRIVATE_FIRST`（`0x1000`），因此你的标识不会与后续公共目录行冲突。使用 `meter_snapshot_read`、`meter_snapshot_parameter` 和 `meter_snapshot_fault_active` 读取域值，使用 `meter_snapshot_fault_set` 写入故障状态；未声明的标识将报告为未知数据。监视器不携带存储：每个监视器都是必须命名已声明信号的呈现行。

设置数据块的大小由你自己的表格决定：`meter_settings_size()` 返回 `METER_SETTINGS_OVERHEAD` 加每个参数四字节，因此调用方传递自行计算的缓冲区，而非平台常量。参数超过 255 的目录需要经评审的版本化格式；平台以返回零长度报告该情况，且不尝试写入。

对于每次下游构建，记录公共平台提交、所有子模块 SHA、SDK 提交、所选产品、镜像 SHA256 和测试日志。公共平台变更经评审后向上游流入私有产品；客户代码永不自动向上游流动。私有单板验证仍为独立的 reference-board 预约，并附原始串口证据。

### Product 语言资源

框架只提供翻译**服务**（`ui/common/i18n/meter_i18n_runtime.h`），措辞属于 Product。框架唯一内置的文字是英文：框架控件自己渲染的
`meter.*` 词汇（`meter.validity.live`、`meter.state.on`、`meter.value.error` 等）。Product 注册含同名标签的包即可覆盖其措辞。

```text
键  →  语言目录（生成的静态数组）  →  文字
```

- **目录**：按 LVGL `lv_translation_add_static` 的版式生成静态包（语言码、以 NULL 结尾的键、键×语言行主序）。运行时没有 JSON 或文件解析；
  来源（JSON、CSV、客户表）留在 Product 里，生成 C 数组。
- **注册**：在 `meter_firmware_composition_t.locale_init` 里对每种语言调用 `meter_i18n_register_language()`（持久化 id、包列名、可选的标签/数值字体），
  对每个包调用 `meter_i18n_register_pack()`。注册幂等，`meter_i18n_init()` 之后每个 LVGL 生命周期注册一次。
- **唯一来源**：持久化的 `snapshot.language` 决定语言。主循环在 `present_update()` 与 `present()` 之前调用 `meter_i18n_set_language()`，
  LVGL 随后向每个对象发送 `LV_EVENT_TRANSLATION_LANGUAGE_CHANGED`。
- **绑定而不是重建**：`meter_i18n_bind_text(label, tag, role)` 一次绑定翻译键与随语言变化的字体；`meter_i18n_bind_font()` 只绑字体。语言没有该角色的字体时，
  对象恢复设计时的字体。
- **回退**：`meter_i18n_tr()` 先取当前语言，再取已注册包的英文列，最后返回标签本身。生成完整矩阵（每列在构建时指定回退语言），绑定的标签就不会显示键名。
- **多于两种语言**：`meter_language_t` 仍是持久化字节（`0` EN、`1` ZH）；Product 用 `meter_catalog_t.language_count` 声明目录接受多少个 id，核心据此校验保存的语言字节、
  语言动作与初始设置。`0` 保持内置的两个 id。用位置初始化器写目录的 Product 现在要补上这个新的末尾字段（指定初始化器不受影响）。
- **溢出文字**：换语言可能让标签变长。给所有译文标签一个统一策略（例如 `LV_LABEL_LONG_SCROLL_CIRCULAR`），不要逐页改版式。

### Product 字体输入

在 **assets/font-config.json** 中使用可选的 **text_sources** 列表声明动态 UI 文本目录。每项都是 Product 内的 UTF-8 源文件相对路径。现有 **translations** 文件仍是主要输入；生成器合并两组输入、去除注释，生成相同的 14/20 px 字体子集。路径必须位于 Product 内。已声明文件缺失时，生成和检查都会失败。

例如将 **translations** 设为 **ui/i18n.c**，将 **text_sources** 设为 **["catalog/display_names.c"]**。运行 **python tools/generate_fonts.py --product-root products/my-product**，再追加 **--check** 检查。生成字体、清单、许可证及输入声明随 Product 一起提交。检查最长标签的实际渲染；字形齐全不代表文字排得下。

### Product submodule

下游仓库可以在 `products/<product>` 下以 Git submodule 引入独立 Product。公共 Framework 仓库本身不能加入客户 submodule。私有下游仓库负责 Product 仓库、客户文档和验证记录。

公开的 `products/demo` submodule 展示这种组织方式。添加私有 Product 时保留 Demo 的固定版本；固件只链接选定的 Product。在私有 Framework 开发分支执行：

```text
git submodule add PRIVATE_PRODUCT_REPOSITORY products/my-product
git add .gitmodules products/my-product
git commit -m "build: pin independent Product"
cmake -S . -B build-my-product -G Ninja -DMETER_PRODUCT_ROOT=products/my-product
```

把 `PRIVATE_PRODUCT_REPOSITORY` 替换为私有 Git URL。新克隆使用 `git clone --recurse-submodules`，已有检出执行 `git submodule update --init --recursive`。先在 Product 仓库提交并推送修改，再在 Framework 开发分支提交更新后的 gitlink。构建使用固定提交，不跟随分支最新版本；可复现构建不要使用 `submodule update --remote`。

元数据豁免仅覆盖 submodule section 名称及 `path`、`url`、`branch` 值。注释和其他文本仍会扫描，凭据检查始终使用完整文件。Product gitlink 内的内容由其独立仓库检查。`products/` 下的普通目录仍属于 Framework 源码，不享受豁免。
