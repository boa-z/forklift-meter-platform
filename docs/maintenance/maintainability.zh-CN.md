# 维护者评估

> [English](maintainability.md)

检查日期为 2026-09-28，应用提交为 `81e508314c5c36ea2e01dc797a8caf29a67f8470`，起始分支 `codex/dynamic-periodic-tx` 且应用工作区干净。本评估从源码重新建立认知，不继承以往的验收结论。下述发现对应此基线；完成情况记录在[执行计划](maintenance-plan.zh-CN.md)。

## 总体评估

平台已具备一致的可移植 Domain 和明确的生产所有者，作为参考应用，其异常路径测试也较充分。应保留这些边界。维护风险主要集中在原生线程编排和紧凑的手写状态机，而不是缺少框架。产品独立性已有宿主证据，但固件组合与硬件验证尚未达到相同程度。它是有价值的工程参考，尚不构成车辆产品合格的证据。

本次检查覆盖构建清单、CI、契约、可移植 runtime/Core、原生所有者启动/循环/停止路径、存储交接、协议适配、升级集成、测试和现行指南。这是聚焦的首次审计，不是逐行穷尽的安全或竞态分析。历史记忆未提供应用实现事实。

## 重建的架构

| 边界 | 实际实现与数据流 | 维护意义 |
|---|---|---|
| Product 组合 | `CMakeLists.txt` 选择 `product/sources.json`；Demo 和两个示例提供目录、路由、能力、策略及 UI。`SConscript` 则显式选择 Demo；`main.c` 包含 Demo 存储和翻译。 | 外部 Product 的宿主构建不能证明固件可替换组合。 |
| 可移植 Domain | `contracts/` 描述身份、值、批次和端口；`core/meter_core.c`、设置和快照助手校验并存储调用方提供的数组。 | 产品身份是句柄而非数组下标。Core 不依赖 OS 或 LVGL。 |
| Protocol | `runtime/meter_runtime.c` 把帧路由到适配器，暂存完整语义批次并报告带身份的命令进度。`protocols/common/` 包含路由；生成的 DBC 适配器归 Product。 | 生产 Protocol 独占协议状态，App 消费复制的语义。 |
| 原生执行 | `platform/rtthread/meter_execution_port.c` 集中 IPC、App/Protocol/TX 入口、模式/代次变更、发布和停止。 | 这是并发组合的主要入口；可移植 runtime 并不等于完整生产调度器。 |
| 周期发送 | App 采样并深复制发布；Protocol 拥有期限、编码器和 wire 状态；逐条目的原生邮箱交给总线 TX 并返回带身份结果。 | 采样、语义修订、发布、准入和完成必须保持区分，不积累历史帧。 |
| 命令与服务 | Product App 工作流提交语义命令；Protocol 路由/编码；总线 TX 报告驱动完成。Mixed SDO 使用固定版本的 CANopenNode 和远端响应。 | APPLIED、TX_COMPLETED、REMOTE_CONFIRMED 是不同证据。本地取消不能撤回远端写入。 |
| UI 与诊断 | `main.c` 独占 LVGL，读取复制的展示数组并提交意图；独立诊断数组和短锁支持 MSH 格式化。 | UI/诊断不能持有可变 Core 数组。初始化先构造 Core，再把修改权交给 App。 |
| 持久化 | `storage/` 提供记录、双槽恢复和 NVM 状态；`meter_nvm_port.c` 将工作/结果交给阻塞 worker。 | RAM 应用、持久化排队和耐久修订不同；存储格式是兼容契约。 |
| 升级 | 公共 Protocol/TX 路径上的 UDS/ISO-TP 接收复制任务；Update worker 校验包，经过 NVM 屏障使用 SDK 原生安装器。 | 哈希完整性不等于身份认证；原生启动确认不等于应用健康确认。 |

代码评审从[运行时所有权](../runtime/runtime-production.zh-CN.md)、[动态 TX](../runtime/dynamic-periodic-tx.zh-CN.md)、[协议](../runtime/protocols.zh-CN.md)、[NVM](../runtime/nvm.zh-CN.md) 和[升级](../ota/can-update.zh-CN.md) 开始。构建清单选择依赖；上游代码保留在固定提交的子模块中。

## 发现与优先级

优先级表示工程执行顺序，不是车辆安全分类。未决事项不授权语义变更。

| ID / 优先级 | 基线证据 | 影响与下一步 |
|---|---|---|
| M-01 / 高 | `meter_execution_port.c` 共 880 行：多个所有者、共享标志、五类队列、周期邮箱、命令账本、发布及生命周期共处一个编译单元。`command_poll`、`periodic_poll`、`app_entry` 和 `copy_traces` 将多个转换压缩到单行。 | 考虑拆文件前先补状态/锁映射和生命周期测试。保留锁、优先级和调用顺序。拆文件本身不是所有权证明。 |
| M-02 / 高 | `runtime/meter_periodic.c` 在校验、期限消费、新鲜度及提交策略之间使用短参数名和多语句分支；现有测试是一个长场景。 | 首个有界加固目标：明确命名/分支并增加具名特征测试，不改算术或策略。 |
| M-03 / 高 | `app_entry` 从 `batch_full + event_full + tx_full` 检测过载；无效语义批次单独增加 `batch_rejected`。运行时指南声称批次/事件/TX 拒绝均进入 DEGRADED。 | 将指南修正为实际计数边界。校验拒绝是否应改变模式属于 D-01 决策，不是清理。 |
| M-04 / 高 | 宿主支持 `METER_PRODUCT_ROOT`；固件 `SConscript` 和 `main.c` 直接指定 Demo。Runtime 接受 Product hooks，但板级入口尚无等效选择。 | 泛化前由 D-02 明确固件 Product 选择、存储绑定、UI 启动和目标源码闭包。 |
| M-05 / 高 | `tests/test_execution_port.c` 包含原生 `.c` 文件并确定性模拟队列；线程初始化/启动、延时和硬件替身被调用即断言失败。`test_publication_threads.c` 在宿主锁下验证发布。 | 这是有效单测证据，但没有执行原生启动、部分失败、停止和优先级调度。补确定性生命周期场景；真实调度和阻塞 I/O 结论仍需目标证据。 |
| M-06 / 高 | CI 可移植分析覆盖 core/runtime/storage/update；生成代码分析另含七个编译单元。手写 Product、platform、UI 和目标配置覆盖不完整，治理文档已承认。 | 以实际编译命令核对第一方编译单元，逐步扩展分析且不增加宽泛抑制。 |
| M-07 / 中 | `check_architecture.py` 从仓库根而非引用文件位置解析 include。`check_runtime_ownership.py` 按函数名切片选择 Protocol，只检查词元而非被调用函数。未注册这些门禁的独立反例测试。 | 相对 include、助手函数和重构可能逃逸或破坏门禁。增强解析/范围前先补合成失败样例；源码检查仍是回归护栏。 |
| M-08 / 中 | 多个 C 测试将初始化调用及校验放入 `assert`；CI 显式使用 Debug，但 CMake 允许含 NDEBUG 的 Release。 | 测试断言尚未机械保证启用前，不应把 Release CTest 全绿视为等价证据。用故意失败样例复现，再增加仅作用于测试的策略，不改生产断言语义。 |
| M-09 / 中 | 双语门禁比较标题/代码块/表格/链接结构，不验证链接存在或翻译含义。部分回调注释仍写 Protocol/App，与生产单所有者规则不一致。 | 修正已确认的所有权措辞，后续增加链接/反例覆盖。双语结构通过不等于技术评审。 |
| M-10 / 高 | `docs/compliance/status.md` 在其检查时记录 TAD-001 待审及缺少受保护必需检查；工作流 YAML 不能强制分支保护。 | 保留待审状态，需要管理员批准和远端实施状态复核；本审计没有查询远端设置。 |
| M-11 / 中 | `docs/build/migration.md` 仍称 CANopen 为分阶段依赖、固件升级不在范围内；其协议/UI 路径及 `docs/product/downstream.md` 中 Demo 存储路径已不匹配源码树。 | 修正文档地图，区分初始提取历史与当前能力，保留历史验证身份。 |

## 产品就绪决策

| 决策 | 所需人工/集成决策 | 关闭前所需证据 |
|---|---|---|
| D-01：拒绝策略 | DEGRADED 继续仅针对 IPC 过载，还是包含无效语义批次、采样发布失败及其他拒绝？定义恢复及诊断预期。 | 产品策略、按计数器区分的测试和目标故障注入日志。现有行为保持。 |
| D-02：固件组合 | 选择 Product 选择和板级绑定契约；保留小型组合入口，不添加通用注册中心。 | 两个独立选择的固件源码闭包、Demo 构建回归、正确 LVGL 链接及匹配板验。 |
| D-03：生命周期失败策略 | 修改原生启停前定义部分初始化清理、禁止重启及耐久屏障失败处理。 | 注入每类 IPC/worker/open 失败；证明不释放存活资源、不将未完成停止报为成功。覆盖每个阻塞后端操作中的停止。 |
| D-04：真实产品验证 | 定义实际协议权威、诊断/安全策略、时序限制、bus-off 行为、NVM 寿命及升级认证/恢复要求。 | 批准的需求及绑定源码/镜像的实板测试。合成 Demo 流量和已有单总线观测不是车辆产品验证。 |

`docs/ota/can-update.md` 已明确且重要的原生升级限制：厂商提前自动确认、未声称具备认证升级或回滚、下载中断电未验证。不得在维护性任务中悄然改变启动信任或恢复策略。

## 验证与置信范围

全新 Windows GNU 16.1.0 / Python 3.13.15 Debug 无界面构建启用可选升级，在编辑前于 `build-maintainer-audit` 通过 **42/42 CTest**。本次从当前源码重建，未复用旧可执行文件。覆盖可移植行为、确定性原生端口路径、公共头文件、生成及仓库护栏。变更后结果和准确命令记录在[执行计划](maintenance-plan.zh-CN.md)。

绑定源码的[验证记录](../testing/validation.zh-CN.md) 仍只为其指定提交和镜像提供历史证据。本审计不会把硬件结论迁移到当前源码。Linux sanitizers/分析器、全新固件构建、实物 UI、双总线硬件、长时间时序及断电测试，均不能由本次宿主运行确认。上层 SDK 和另一应用检出已有状态；本任务不更改配置、gitlink 或相邻源码。
