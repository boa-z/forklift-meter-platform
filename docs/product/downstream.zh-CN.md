# 私有下游指南

> [English](downstream.md)

从已打标签的公共平台版本启动新的私有仓库，并设置：

```text
origin   → private product repository
upstream → https://github.com/boa-z/forklift-meter-platform
```

仅在 `products/<customer>`、`protocols/vendor/<customer>`、`ui/products/<customer>` 之下以及私有资源/文档目录树中添加客户代码。添加产品组合记录和选定源列表。将客户目录、捕获文件、对象字典、维护对象和 UI 资源排除在公共仓库之外。除非经评审确认确实缺少某项通用能力，否则不得向 `contracts`、`core`、`runtime`、`protocols/common` 或 `ui/common` 添加客户符号或条件。

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

### Product submodule

下游仓库可以在 `products/<product>` 下以 Git submodule 引入独立 Product。public-clean 会把 `.gitmodules` 中的路径和 URL 当作依赖元数据，并跳过 gitlink 指向的外部内容；同时仍检查凭据，并扫描所有第一方源码。公共 Framework 仓库本身不能加入客户 submodule。私有下游仓库负责 Product 仓库、客户文档和验证记录。
