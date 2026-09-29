# 当前维护计划

> [English](maintenance-plan.md)

根据[源码评估](maintainability.zh-CN.md) 于 2026-09-28 建立，基线 `81e5083`。这是当前续接记录；随代码变更同步更新此处的状态与证据，不依赖聊天历史。

## 行为保持加固

| 顺序 / 项目 | 范围与理由 | 完成证据 | 状态 |
|---|---|---|---|
| H-01 | 记录架构及差异；从 AGENTS 和文档索引链接计划。按现有源码修正 M-03、M-11 和已确认的所有者注释。 | 双语门禁、源码评审、策略未改变。 | 完成 |
| H-02 | 使 `runtime/meter_periodic.c` 可局部理解：描述性参数、展开分支、明确阶段注释。为发布拒绝原子性、编码拒绝/身份及身份耗尽增加独立测试。 | 五个新增具名用例在原实现通过；保留已有生命周期断言；完整 Product 矩阵通过；优化对象相同。签名、布局、算术、期限和策略不变。 | 完成 |
| H-03 | 为架构/所有权护栏建立反例（M-07），再增强相对 include 处理。保留所有已有门禁及范围。 | 样例拒绝刻意违反、允许合法 include、扫描锚点缺失时报清楚错误；在 CTest/CI 注册。按治理规则记录门禁变更。 | 完成 |
| H-04 | 保证 C 测试初始化/检查不因 NDEBUG 消失（M-08）。 | Debug 和 Release 均拒绝故意失败的测试；仅影响测试；所有 Product 矩阵通过。 | 完成 |
| H-05 | 记录原生共享状态/锁所有权，逐步增加启停故障注入接口（M-01/M-05），不移动 worker 或改变同步。 | 测试覆盖已初始化资源和确认、队列拒绝及耐久阻塞；区分确定性替身与目标结果。 | 可移植特征验证完成，D-03/目标证据开放 |
| H-06 | 清点分析器覆盖，按可评审分组扩展手写第一方范围（M-06）；验证文档链接和生成器失败路径（M-09）。 | 范围工件核对选中源码及编译命令；分析发现修复或明确待人工评审。 | 已增加明确范围，CI/目标及其余工具工作开放 |

H-01/H-02 保留下方原始验证记录。2026-09-28 续接工作使用外部真实产品需求检验这些优先级。每批先读相关契约、编辑前建立特征测试、保留外部语义、运行直接相关测试及选定 Product 矩阵并记录限制。避免大范围格式变动或新增抽象层。

## 需求驱动的续接工作

公共仓库只保存可复用结论。私有需求文档已作为证据阅读，不复制到源码、目录或样例。本地忽略的证据记录保留文档哈希和待定 ID。所有 P0 仍未解决：A1/A2/A3、B1/B2/B3/B4、C1/C2、D4、E1、F3 和 G1。条目内建议不代表批准。总线分配、标定前提、计数策略及默认值即使被来源标为 P1 也仍需确认。本次不引入真实产品协议、密码、故障表或配置。

| 能力 | 当前实现 / 实际缺口 | 下一步复用工作 |
|---|---|---|
| Product 组合 | 宿主及固件均在编译期选择唯一 Product，通用启动使用组合契约。 | D-02 启动耦合已关闭，独立验证目标预算/适配。 |
| 双 CAN 与周期 RX/TX | 帧及路由键已包含总线/格式/ID；周期定义允许独立周期。板级拓扑/波特率仍由集成固定。 | F-03：增加重叠 ID、超时/恢复及 20/50 ms 合成回归。物理时序和总线分配尚未验证。 |
| 值/来源/新鲜度 | Core 在 VALID 变 STALE 时已保留值和来源，初始为 UNKNOWN。ERROR 可替换无效值；无独立最后有效值历史。 | 复用 Core 超时。另行决定是否需要最后有效值历史和消息级监督，不将 UNKNOWN/STALE 显示为有效零。 |
| 参数 | Core 数字 ID 标识本地持久化设置；命令账本和 SDO 调度器不提供可复用的属主限定参数目录/结果边界。 | F-02：独立 App 所有的参数服务，复用请求身份/账本；显式请求/尝试关联及保留的类型化结果。不改已有设置或线上格式。 |
| 权限 | 现有 auth profile 仅有两个静态布尔值；没有可到期/撤销授权。 | F-02：准入和派发检查显式权限与会话代次；Product 负责凭据。现有 UI 策略不变。 |
| 设置持久化 | 已有默认值、校验、命名空间/模式及双槽恢复；未知模式阻止覆盖；无迁移注册机制。 | 复用存储原语；适配器改变前由 D-05 批准迁移映射和持久表示。 |
| 小时计与距离 | 尚无仪表累积/检查点/复位服务。 | F-05：存储集成前明确单位、不连续、溢出、复位权限及检查点确认。Product 提供使能和频率，不设通用 Flash 间隔。 |
| 诊断 | Domain、CAN、storage、trace 和周期计数已区分部分原因；通用拒绝计数仍混合部分原因。 | F-02 增加类型化参数结果。D-01 保持显式产品健康转换；后续区分其他入口/契约失败，不把全部拒绝映射为 DEGRADED。 |
| 控制器配置与能力 | Product 提供静态能力标志和来源策略；无原子动态配置/能力发布契约。 | F-06：规定配置代次失效和规范能力；Product 映射品牌/功能位。 |
| 标定 | 已有 Product App 工作流；无基于新鲜测量和参数结果的可复用标定生命周期。 | F-07 在 F-02 后描述前提、新鲜度、写结果不确定性和可选回读，不在 LVGL 编排事务。 |
| 身份/版本 | 已有诊断构建身份，但 Product/升级/控制器身份表示分散；不支持读取需显式结果。 | F-08：改变 CANopen/升级接线前提出消费者共享的只读提供者。 |
| CANopen 维护 | Mixed 提供独立 SDO client；产线维护 server 不属于本框架。 | 通道/OD 需求确认后独立适配传输，不推断 NMT/Heartbeat 或引入完整 DTC 系统。 |

当前有界交付：F-02 参数/权限契约和确定性测试；F-03 复用/反例验证；为这些测试补 H-03 门禁反例和 H-04 断言保证。F-02 保持可选内部代码，不绑定生产后端。固件选择、持久化模式/迁移、车辆健康策略及 worker/时序变化仍需显式决策。后续能力仍在本计划维护，不新建竞争路线图。

## 架构决策

| 项目 | 建议下一步 | 决策人 / 状态 |
|---|---|---|
| D-01 拒绝策略 | 评审实际计数器，决定语义校验拒绝是否影响模式。决定前只记录现有行为。 | 产品/维护者；未决 |
| D-02 固件 Product 选择 | 唯一编译期 Product 所有静态存储及本地化，Demo 与 Reference-B 独立编译，默认行为不变。 | 组合耦合已关闭，目标适配证据开放 |
| D-03 生命周期失败 | 改变所有权或停止保证前决定清理/重启/耐久失败语义。H-05 可独立描述现有行为。 | 维护者；未决 |
| D-05 持久化迁移与计数语义 | 保留未知模式不覆盖及双槽恢复；评审已知旧模式迁移、计数单位/使能/复位/检查点和掉电预算。未确认映射与默认值不得写入。 | 产品/维护者；未决 |
| D-06 Settings/Parameters 词汇 | 本地权威 Settings 名称/MSP2 与远端属主限定事务保持独立，不公开重命名/迁移。 | 保留兼容性 |
| D-07 Product App/UI 绑定 | 原生绑定前评审已确认后端关联/排空、认证及已离开面板/配置的生命周期。 | Product/维护者；开放 |
| 治理实施 | 按合规状态文档评审现有 TAD-001 并配置/核实必需检查。没有 agent 批准或宽泛豁免。 | 人工评审者/仓库管理员；待处理 |

这些是建议，不是已接受的架构决策。作出决策时记录理由、替代方案、兼容性及验证。本计划不授权改变 CAN/协议行为、时序保证、线程所有权、存储格式、升级信任/恢复或诊断/安全策略。

## 产品功能工作

真实车辆协议集成、客户 UI/素材、产品诊断/安全规则和升级认证/恢复属于独立 Product 工作。评估中的 D-04 定义需求缺口。不得把私有内容导入公共参考仓库。宿主 Reference-B/Mixed 成功不关闭固件适配或双总线验收。

## 当前批次验证

基线全新 Debug 无界面 + 升级构建：**42/42 CTest PASS**，Windows GNU 16.1.0 / Python 3.13.15。从应用根目录复现：

```sh
cmake -S . -B build-maintainer-audit -G Ninja -DMETER_BUILD_UI=OFF -DMETER_ENABLE_UPDATE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-maintainer-audit -j 8
ctest --test-dir build-maintainer-audit --output-on-failure
```

基于 `81e5083` 的未提交 H-01/H-02 批次变更后结果：

| 配置 / 检查 | 结果 | 本地证据 |
|---|---|---|
| Debug 无界面 + 升级 | 42/42 PASS | `build-maintainer-audit/Testing/Temporary/LastTest.log` |
| 全新 Demo SDL | 44/44 PASS | `build-maintainer-audit/demo-ctest.log`, `build-maintainer-demo/test-results.xml` |
| 全新 Reference-B SDL | 34/34 PASS | `build-maintainer-audit/refb-ctest.log`, `build-maintainer-refb/test-results.xml` |
| 全新 Reference-Mixed SDL | 33/33 PASS | `build-maintainer-audit/mixed-ctest.log`, `build-maintainer-mixed/test-results.xml` |
| Python 宿主工具，含真实原生成包工具 | 100 通过，9 个物理 HIL 跳过 | `build-maintainer-audit/python-venv-tests.log`，同目录 `python-venv-results.xml` |
| 原周期实现运行最终特征测试 | PASS | `build-maintainer-audit/periodic-original-characterization.exe` |
| 周期优化对象变更前后 | GNU 16.1.0、C11、O2、无调试信息时字节相同 | `build-maintainer-audit/periodic-before.o`, `periodic-after.o` |
| 格式与仓库护栏 | 修改的周期文件满足 clang-format；架构、所有权、公共头文件/清洁性及 26 对双语文档通过 | CTest 日志及最终本地检查 |

SDL 矩阵使用相同 Debug CMake/build/CTest 流程，启用 UI，`METER_PRODUCT_ROOT` 分别为 `products/demo`、`examples/reference-b`、`examples/reference-mixed`，并使用独立构建目录。本 Windows 主机通过 `SDL2_DIR` 选择 SDL2，CTest 的 PATH 增加其运行库目录；测试使用 dummy 视频驱动。自动化检查不构成人工视觉评审。

首次系统 Python 运行有 96 通过、1 失败（缺少 `isotp`）、12 跳过，保留在 `build-maintainer-audit/python-tests.log`。成功重跑使用现有测试虚拟环境中的固定版本 HIL/OTA 依赖，并显式指定 `METER_OTA_INSPECTOR`、`METER_OTA_CPIO` 和 `METER_OTA_MKENVIMAGE`。没有为获得最终结果弱化测试或阈值。将这些变量指向当前构建/原生工具后运行：

```sh
python -m pip install -r tools/hil/requirements.txt -r tools/ota/requirements.txt
python -m pytest -q -ra
```

对象共享 SHA256 `9bf4e998b6baedb2748863bf7f5eeb668dbf059852ad716d8341c0d09f83cfc9`。这仅是该宿主编译器的证据，不是目标时序测量。原始构建/配置日志及结果清单保留在忽略的 `build-maintainer-audit` 目录；本表是仓库中的持久摘要。不暗示当前源码固件/HIL、Linux 分析器或 sanitizer 已通过。已有[绑定源码的硬件证据](validation.zh-CN.md) 保留原始身份。

## 需求批次结果

上一阶段审计已独立提交为 `c3ddb09`，SDK 在 `81e0408b` 固定该引用。续接工作在两个仓库的 `codex/product-framework-hardening` 分支进行，不包含兄弟私有应用的修改。上方 H-01/H-02 原始证据保留为历史记录。

| 项目 | 已交付范围 | 剩余边界 |
|---|---|---|
| H-03 | 真实 CLI 反例；本地双引号 include 和已知 Product 根解析；锚点缺失/重复/倒置时报清楚错误 | 词法检查不证明传递调用、宏展开或并发安全，GCR-002 人工评审仍待处理 |
| H-04 | 仅测试取消 NDEBUG 并增加断言失败见证；完整 Debug 矩阵和全新 Release 通过 | 生产优化/标志不变，未来测试命名/编译器改变时需评审 |
| F-02 | 属主限定数值目录、未确认条目默认拒绝、权限授权、已有账本、有界读重试及类型化保留结果 | 仅可选内部接口，无生产适配器、凭据、协议编码、持久化模式或 UI 接线 |
| F-03 | 重叠总线/格式身份；UNKNOWN/STALE/ERROR、值/来源保留及恢复；独立 20/50 ms 调度 | 描述已有实现，不代表物理总线/时序/控制器验收 |

[参数契约](parameter-service.zh-CN.md) 明确所有权、就绪状态、后端隔离、取消不确定性、浮点表示及期限限制。合成测试覆盖结果保留、错误属主/会话/序号/操作/尝试号、权限寿命、时钟回绕和耗尽。本地身份比较成功不能替代线上回复关联。

| 当前源码检查 | 结果 | 证据 |
|---|---|---|
| Debug 无界面 + 升级 | 46/46 PASS | `build-maintainer-audit/build-maintainer-audit-final-ctest.log` |
| Debug Demo SDL | 48/48 PASS | `build-maintainer-audit/build-maintainer-demo-final-ctest.log` |
| Debug Reference-B SDL | 38/38 PASS | `build-maintainer-audit/build-maintainer-refb-final-ctest.log` |
| Debug Reference-Mixed SDL | 37/37 PASS | `build-maintainer-audit/build-maintainer-mixed-final-ctest.log` |
| 全新 Release 无界面 + 升级 | 46/46 PASS | `build-maintainer-audit/build-maintainer-release-final-ctest.log` |
| Python 宿主工具 | 107 PASS，9 项物理 HIL 跳过 | `build-maintainer-audit/framework-python.log` |
| 文档 / 格式 / 范围 | 27 对双语文档、public-clean、头文件、架构和所有权 PASS；修改的 C 满足 clang-format | 各构建 CTest XML 及本地最终检查 |

Release 复现使用上方无界面配置，加 `-B build-maintainer-release -DCMAKE_BUILD_TYPE=Release`，随后构建和 CTest。已有 SDL 构建目录执行增量重建。证据来自 Windows GNU 16.1.0 和 Python 3.13.15；Linux sanitizer/分析器及当前源码固件/HIL 均为 NOT_RUN。首次门禁反例发现 Windows 路径分隔符问题；规范化诊断后重跑完整矩阵，未移除检查。

下一批安全工作为 H-05 所有者/生命周期特征验证，以及基于 F-02 的 F-07 标定工作流规格；F-05 计数、F-06 配置/能力发布和 F-08 身份提供者仍待设计实施，不是已有功能。D-01/D-02/D-03/D-05 及全部未确认需求保持开放。真实产品适配在任何生产参数写入前必须确认描述符，并完成后端关联/排空测试。

新增两个 runtime 源文件另通过宿主 GCC `-fanalyzer -Wall -Wextra -Werror`；这不替代 CI 的 Clang/Cppcheck 或目标分析。`build-maintainer-audit/framework-evidence.json` 记录工具版本与源码 SHA256，最终矩阵 XML 为各构建的 `final-framework-results.xml`。

## App 集成续接

已接受基线：`codex/product-framework-hardening` 的 `7ed094f`；维护者报告 host/quality 已绿。继续本计划，不重新审计。当前按依赖排序：先描述原生启停及失败所有权，再以独立 Product 证明编译期固件组合，随后用合成后端验证可选参数 App 集成及复制的展示值。增加 Release/无界面 CI 并明确扩展手写源码分析范围，不缩小已有范围。

续接结果：下方已实现 H-05 可移植生命周期特征验证、D-02 单 Product 组合及 F-02 App/展示见证。D-03 和 D-07 保留未决生产策略，未改变私有线上编码、凭据、物理时序或持久化格式。

### H-05 生命周期证据

独立进程故障注入覆盖全部 21 个原生资源初始化失败、App 启动失败、TX0/TX1/Protocol 启动失败、NVM 启动失败及分阶段耐久/工作线程/UI 停机确认。原有 IPC 测试保留队列饱和及复制发布覆盖。将 App 所有的停机握手提取为一个具名操作前后，相同 28 个生命周期/IPC 测试均通过。没有移动工作线程、锁、队列、延时或超时策略。宿主对象散列不同，不宣称二进制相同或目标时序证据。

D-03 证据：部分初始化保留已创建原生对象，但 `initialized` 仍为 false，重试会再次访问它们。App 线程启动失败留下 initialized/FAILED 状态且没有 App 所有者驱动停机。正常停机为一次性生命周期。耐久或工作线程确认永不到达时可无限等待，5 秒诊断不授权强制销毁。改变这些路径前，需结合 RT-Thread 资源语义决定有界失败/重启/重试策略。测试描述现有保留行为，不认可重试。可选 OTA 工作线程生命周期和真实 RT-Thread 调度不在本桩测试范围内。

CI 已增加 Release/无界面加 update 的构建和 CTest 步骤。后续批次完成时记录本地全套结果及扩展分析来源。

### D-02 编译期组合结果

已按授权的编译期方向实现，默认 Demo 运行行为不变。`contracts/meter_firmware.h` 绑定四份独立静态所有者存储及 UI 所有者的本地化初始化函数。Demo 保留相同存储类型/容量，并在相同启动位置调用原本地化初始化。Reference-B 提供独立的纯信号存储及其原 UI 翻译注册。通用 `main.c` 不再包含 Product 实现头文件，由所有权反例保护此边界。未引入运行时插件，也未改变协议路由、线程、时钟、存储格式或公开 Product 结构。

SCons 经 `tools/firmware_product.py` 选择 `METER_PRODUCT_ROOT`（默认 `products/demo`），相对路径以应用根目录解析。选择器拒绝缺失/越界/重复源码及尚不支持的特性闭包。Reference-Mixed 不启用固件组合，其 SDO 闭包需独立板级集成决策。可用 `python tools/firmware_product.py --product-root examples/reference-b` 查看选择，此命令不构建或烧录。

Demo 与 Reference-B 分别编译同一通用固件入口，并对真实目录、core 和 LVGL 链接/执行所选组合。测试验证容量、所有者数组独立、快照复制及本地化初始化。这关闭了 Demo 专属启动耦合，并为 D-02 提供具体审阅证据。目标链接、内存预算、显示/CAN 板级适配及实板验收仍未验证，宿主通过不代表可发布产品。

### 本地 Settings 与远端 Parameters

| 现有概念 | 权威 / 含义 | 兼容边界 |
|---|---|---|
| `meter_parameter_def_t`、快照参数数组 | App 所有的本地权威 Settings，目录 ID 定位本地工程值 | 保留名称/布局及 MSP2 记录，数组赋值不等于远端写入 |
| `METER_ACTION_PARAMETER` | UI 修改本地设置的意图，由 App 校验应用 | 原准入/持久化语义不变，不派发远端事务 |
| `meter_auth_profile_t` | Product 对本地设置和车辆控制的静态策略 | 不是登录、凭据或有期限授权 |
| `meter_parameter_definition_t`、`meter_parameters_t` | 所有者限定的远端描述符及保留事务结果 | 确认合成描述符不授权真实线上映射，不隐式修改快照/NVM |
| `meter_authorization_t` | App 所有的可过期/撤销事务权限 | 由 Product 认证提供，不持久化凭据，也不从静态布尔值推导 |

D-06 兼容性处置：保留现有公开名称及序列化表示。重叠是语义词汇，不授权合并存储或权威。新文档及边界示例用 Settings 表示本地值，Parameters 表示远端事务。未来公开重命名、迁移或远端到本地缓存须先说明调用方兼容、记录版本、新鲜度及确认策略。本批无需此类迁移。

每个实际固件镜像只选择一个 Product。单一 `METER_PRODUCT_ROOT` 解析一份清单及唯一 `meter_product_get` / `meter_firmware_compose` 实现。Demo 与 Reference-B 用独立构建目录和可执行文件验证，绝不组合进同一固件或使用运行时选择器。本地组合批次：Demo 77/77、Reference-B 67/67 CTest 通过，证据为 `build-maintainer-audit/demo-composition.log` 及 `refb-composition.log`。

### App 边界与分析结果

F-02 现有 `examples/parameter-workflow` 中的合成 App/传输/UI 边界见证，详见[参数服务指南](parameter-service.zh-CN.md)。它仅用于测试，不增加固件 Product，也不改变既有运行时回调。D-06 保留本地 Settings 名称/格式。D-07 仍是人工/Product 集成决策：生产绑定前确认描述符、认证、后端隔离及面板/配置生命周期。不选择私有编码、凭据或安全/健康策略。

H-06 增加七份明确手写源码：诊断、trace、通用路由、诊断命令、可移植 RT-Thread 适配、原生执行所有者及参考参数 App。`tools/analyze_handwritten.py` 记录源码 SHA256、实际分析器版本、命令、结果及包含上下文，保留原 portable/generated 范围。本地 Cppcheck 七份均通过。原生分析采用关闭 OTA 的宿主 RT-Thread 桩，不宣称本地执行了 Clang-tidy/Linux CI 或目标编译器分析。host 工作流保留原作业并增加 Release/无界面加 update 及明确范围分析。GCR-003 记录门禁变化。

生命周期剩余边界：D-03 下仅描述部分初始化保留、App 启动失败及 STOPPED 一次性行为，未作改变。`state_lock` 保护状态元数据，`tx_publication_lock` 保护周期发布复制，`view_lock` 保护 App/UI 快照复制。确定性 App 桩等待其他所有者确认时不持有锁。线程栈、队列及信号量存储保持静态，只有 App 推进停机握手，各工作线程和 UI 确认自己的完成。可选 OTA 生命周期及真实调度/资源回收仍需独立证据。

### 集成批次验证

| 配置 / 检查 | 结果 | 证据 |
|---|---|---|
| Debug 无界面加 update | 75/75 PASS | `build-maintainer-audit/audit-integration-tests.log` |
| Debug Demo SDL | 78/78 PASS | `build-maintainer-audit/demo-integration-tests.log` |
| Debug Reference-B SDL | 68/68 PASS | `build-maintainer-audit/refb-integration-tests.log` |
| Debug Reference-Mixed SDL | 66/66 PASS | `build-maintainer-audit/mixed-integration-tests.log` |
| Release 无界面加 update | 75/75 PASS | `build-maintainer-audit/release-integration-tests.log` |
| Python 宿主工具 | 112 PASS；9 项实板 HIL 跳过 | `build-maintainer-audit/integration-python.log` |
| 七份手写源码 | Cppcheck 与 GCC `-fanalyzer -Wall -Wextra -Werror` PASS | `build-maintainer-audit/handwritten-cppcheck.json`、`handwritten-gcc.json` |
| 仓库门禁 | 双语、公开头/清洁、架构及所有权（含新反例）PASS | 各构建目录 CTest `integration-results.xml` |

沿用前述 CMake 配置，分别构建并运行 CTest；Release 为启用 update 的无界面配置，各 SDL 目录只选择一个 Product。`python tools/analyze_handwritten.py --tool cppcheck --output build-maintainer-audit/handwritten-cppcheck.json` 复现本地 Cppcheck。CI 不传 `--tool`，要求 Cppcheck 和 clang-tidy 全部运行。本地结果来自 Windows GNU 16.1.0 / Python 3.13.15。GCC 暴露 Shell 桩缺少导出引用后修正桩，并重建重跑原生生命周期测试，未放宽警告策略。

提交 `f30ff95` 和 `e07f899` 分离生命周期特征验证与单 Product 组合。当前树 Linux Clang/消毒器/模糊 CI、目标固件链接、真实控制器互操作、物理时序、UI 视觉验收及 HIL 仍为 NOT_RUN。维护者报告的绿色 CI 属于基线 `7ed094f`，不是本批。下一步按 D-03/D-07 决策或独立 H-06 工具工作继续，不授权线上编码、凭据、存储迁移或健康策略变更。

## Application services continuation

当前里程碑：基于既有快照的 Product 自有、可无界面测试的呈现模型；规范化运行时 profile 代际；基于既有参数服务的有界应用标定流程；与健康策略分离的事件分类。均为增量可选服务，不建立第二套运行时或快照引擎。

按依赖推进：先迁移 Demo 投影；定义并验证代际失效；使用合成传输证明标定及结果保留；明确 D-01/D-05 和身份提供者决策。每个固件仅一个构建期 Product，保留全部矩阵。不重构原生 worker、不引入私有协议映射或凭据、不实现持久计数器、不变更安全策略。

私有需求仅作证据。新鲜度、功能可见性、随 profile 变化的故障及版本选择、采样写参数的标定需求驱动这些边界；源文件中未决冲突保持未决。以下逐批记录不变行为、确定性验证和实板限制。

### 服务批次结果

Demo 使用无头 Product 投影；profile generation、标定及诊断分类均为可选契约。[Application 服务](application-services.zh-CN.md) 记录所有权、重编译兼容性、D-01/D-05 决策表及身份评估。D-03 不变；D-07 生产映射/认证/缓存失效仍待决策。未实现持久计数器和客户适配器。

| 检查 | 结果 | 证据 |
|---|---|---|
| Debug 无头 + update / Demo SDL | 79/79 与 82/82 PASS | build-maintainer-audit/build-maintainer-audit-services-test.log 和 build-maintainer-demo-services-test.log |
| Reference-B / Reference-Mixed SDL | 71/71 与 69/69 PASS | build-maintainer-audit/build-maintainer-refb-services-test.log 和 build-maintainer-mixed-services-test.log |
| Release 无头 + update | 79/79 PASS | build-maintainer-audit/build-maintainer-release-services-test.log |
| 十个显式手写源文件 | Cppcheck 与 GCC analyzer PASS | build-maintainer-audit/services-cppcheck.json 和 services-gcc.json |
| 仓库检查 | 28 组双语文档；架构、所有权、公开头文件/纯净性及负向夹具 PASS | 各构建 services-results.xml |

仅为 Host GNU 16.1.0 / Python 3.13 证据，不隐含当前源码 Linux CI、sanitizer/fuzz、物理时序或 UI 视觉验收。首轮 Python 缺少 can-isotp；隔离环境使用固定依赖重试，在此记录首次缺依赖失败。源码提交后另记实板验证。

Python Host 工具：114 PASS，9 项物理 HIL 跳过；证据 build-maintainer-audit/services-python.xml 与 services-python.log。通过既有 METER_OTA_CPIO/METER_OTA_MKENVIMAGE 环境覆盖使用 SDK 原生 cpio/mkenvimage，打包测试无跳过。

### 服务实板收尾与下一步

提交 1cf7aea（profile）、5342d4b（Product 展示）及 5cfa0bf（标定/分类）分为独立评审批次。已提交候选完成 reference-board 构建、CAN OTA 和源码身份核对重启，物理 HIL 9/9 通过；见[验证记录](validation.zh-CN.md)。板上保留 services-a，两个接口均已释放。默认 runtime/协议/时序/存储策略不变；新服务仍须 Product 显式接入。

后续继续本计划：真实适配器前评审 D-07 Product 映射、测量/profile 失效和认证；修改健康策略或实现持久计数器前决定 D-01/D-05 表；保持 D-03 所有权/恢复语义不变。把既有 iso14229 文档子模块元数据列为依赖清单问题。本地未运行当前 HEAD Linux CI；不新增路线图、私有线端映射或自动依赖修复。

### Product 适配边界跟进

从 67a378a 继续，不新增 Framework 机制。Demo 投影、归一化 profile、标定服务和分类器已具备基础。参考 UI 尚泄露 owner-qualified key 与完整事务结果。批次 A 将地址选择移入合成 Product App，展示仅保留语义字段与纯结果值。不改变 Runtime/公开契约或生产固件。确定性测试保留 owner 隔离、面板/profile 生命周期、权限/排空规则，并区分有效零值与不可用/拒绝结果。

批次 B 已以一条无头 Product 适配示例贯穿既有服务：profile 发布前使旧测量失效，App 归一化功能可见性，仅捕获匹配采集代际的数据，profile 替换后保留不确定写入，后端排空后完成回读。策略均为示例；D-01/D-03/D-05/D-07 生产决策继续开放。

批次 A 验证：全新 Debug/headless 构建中的参数 App、公共头文件、架构及负向守卫用例通过；有效零值与被拒绝的结果副本明确区分。AGENTS.md 现要求说明性代码注释使用中文，新增服务契约已单独提交注释修正。本批不新增硬件验收结论。


批次 B 仅增加测试内的 Product App/ViewModel 组合，详见 [Application 服务](application-services.zh-CN.md)。映射和策略仍是显式 Product 代码，不新增通用运行时接口或生产线程。用例验证采集代数拒绝、发布前一致失效、保留不确定旧结果、独立后端排空、捕获值稳定性、可选回读及权限丢失分类。

| 检查 | 结果 | evidence/adaptation/ 下的本地证据 |
|---|---|---|
| Debug headless + update | 80/80 PASS | headless.xml；最终示例重建复测见 headless-recipe.xml |
| Release headless + update | 80/80 PASS | release.xml；最终示例重建复测见 release-recipe.xml |
| Demo / Reference-B / Reference-Mixed SDL | 83/83、72/72、70/70 PASS | demo.xml、reference-b.xml、reference-mixed.xml |
| Python 宿主工具 | 115 PASS，9 项物理 HIL 跳过 | python.xml；隔离 Python 3.13 环境使用固定版本依赖 |
| 手写代码分析基线 | 十个源文件，Cppcheck PASS | handwritten.json 和 handwritten.log；保留原范围 |
| 参考 App 及集成示例 | Cppcheck 和 GCC 分析器 PASS | adaptation-analysis.json；源码哈希、版本、命令及原始诊断 |
| 格式、公共头文件/洁净、架构和文档 | PASS；28 对双语文档 | 构建矩阵日志、boundary-test.log；未移除检查或阈值 |

工具链：Windows、GNU 16.1.0（无界面使用 WinLibs；SDL 使用 MSYS2）、CMake 4.4.3、Python 3.13.15、Cppcheck 2.21.0、clang-format 23.1.1。SDL 首次自动选中未准备依赖的 MSYS2 Python；初次失败已归档，重新配置后显式选择固定依赖的隔离环境。扩展分析发现新测试断言包含副作用，已将调用移到断言之外并复测，没有添加抑制。首次诊断采集还遇到 Windows 混合输出编码，精确原始字节已保留。修正测试源码后，两种无界面模式均重建并复测了该示例。这些结果不代表当前提交的 Linux CI、sanitizer/fuzz、UI 渲染验收或新增硬件测试。本批未访问开发板，已有 services-a 证据仍绑定其原始源码。

下一步 Product 工作应提供已确认的映射/目录、能力规范化、采集身份、认证策略、ViewModel 及 UI 组合。D-07 的真实传输关联/排空及失效/认证策略、D-01 的健康后果、D-05 的计数语义与持久化、D-03 的原生恢复/等待契约仍待决策，合成示例不构成隐式批准。只有具体适配缺口证明可复用且能降低复杂度时，才考虑增加共享机制。

### Demo UI 可读性调整

Demo UI 现在按参考的 800×480 仪表几何重新布置：固定 53 px 顶栏、372 px 内容区和 55 px 导航区。主要数值保留大字号，Product 标签、页面标题、监测行、故障行和设置控件统一提升字号。卡片与状态行增加垂直留白，既有语义展示边界、四页导航、翻译和控件行为保持不变。

中文字体解析器对标签和数值展示统一使用仓库内固定的 20 px 子集，因此没有引入未审查的字体来源，也没有改变字体生成契约。英文使用现有 LVGL 16/20/24 px 字体。SDL 截图以及场景、双语和 fixture 测试构成宿主侧视觉与行为证据；暂不宣称实板渲染验收。
### Demo UI 翻页整理

监控、故障和本地设置页现在在既有 800×480 画布内使用明确的两页布局。监控和故障条目按页显示并隐藏非当前条目；设置页将偏好选择与数值控制分开。上一页/下一页按钮在边界处禁用，同时保留原有快照、动作和翻译契约。新增确定性 UI 测试覆盖中英文、有效/过期/错误值、翻页保持、边界、终止按钮和被拒绝的设置动作。目前只有主机证据，实板视觉验收仍待完成。

本翻页批次由 Framework 提交 `005e157` 和场景覆盖提交 `973795f` 组成，SDK 在 `cfda3cf0` 固定了 `973795f`。Debug Demo CTest 通过 84/84，包含新的 `ui-pagination` 测试。候选镜像使用 SDK Python 3.8/SCons 入口构建，并通过既有 CAN OTA 路径安装为 `demo-ui-20260929`；重启后的 UART 与 CAN 身份均匹配新的 Framework 源码。未采集开发板屏幕，因此实板渲染页面视觉验收仍为 `NOT_RUN`。

### Demo 产品适配 UI 与权限

Demo 现在把页面明确分成参数监控和控制器参数设置两项功能。两者都通过 Product 展示数据和真实翻译标签生成，UI 代码不再拼接中英文混合标题。控制器设置使用 Product 自有的合成适配器，验证 `METER_ACTION_PRODUCT`、owner 限定参数准入、授权到期和结果发布；其中不包含真实私有 CAN 编码。

设置页固定为四页：四项本地用户设置、密码页、四项管理员设置和仪表版本页。用户密码 `1234`、管理员密码 `5312` 仅用于演示，不是生产凭据。管理员设置在 App 的限时授权有效前保持隐藏。版本页在可用时显示固件和构建身份，主机环境明确显示回退身份。

新增的 `services/settings_app.c` 由 App 独占权限与参数服务状态，并通过现有快照 revision 机制发布复制后的语义值；本地权威 Settings 仍与远端 owner 限定 Parameters 分离。`demo-settings-app` 覆盖错误密码、不同 owner、无效/越界值、排队和完成结果、退出登录、授权到期、时钟回绕及 reset 不改变本地设置数组。

主机证据：重新生成目录和字体后，Debug Demo 的 `demo-settings-app`、`ui-pagination`、`i18n-ui`、`sdl-smoke`、固件组合、架构和 public-clean 检查通过。SDL 截图保存在 `evidence/adaptation/settings-captures/`。截图仅是主机证据；目标开发板的触摸操作、实屏效果和键盘交互需要下一个 OTA 窗口验证。


实板验证 2026-09-29：OTA 候选 demo-ui-auth-20260929，包 SHA256 835DD979FB9E520D6E8BA6CF97B214D28484CD5B0962F8FF967A31EC32F4CD64；原生构建通过、包预检通过，COM11/PCAN_USBBUS1 实体 HIL 9/9 通过。升级前为 demo-ui-20260929，重启后上报 demo-ui-auth-20260929、reference-demo/reference-board、CAN 500000，领域信号 25、参数 10、故障 10，UI present/flush 持续增长，存储 READY。原始 UART/CAN 证据在 evidence/ota/demo-ui-auth-20260929/board2/。本批次未执行触摸键盘、密码输入和实屏视觉验收。


### Demo 仪表 PDO 后续批次

已接受 UI 基线为 418a123。通过现有 App 发布和 Product 编码器新增两路明确的合成、仅发送 Demo PDO，保留现有 0x3C0/50 ms 和 0x2F0/100 ms 帧。接收 DBC/生成器与 Core 契约不变；独立发送 DBC 描述新增线协议。它们是周期过程数据示例，不代表 CANopen NMT/SYNC/对象字典实现或客户映射。

顺序：以确定性 Product 测试保护精确载荷、新鲜度、计数器与调度行为；增加实物 CAN 接收/恢复检查；构建单一 Demo 固件，通过现有 OTA 安装，保存身份、哈希和原始日志。不扩大当前静态 TX 预算。保留源采样时间，显式标记过期数据。结果回填本节；目标屏幕视觉验收与 CAN/HIL 验收分别记录。

Host 验证：Demo Debug 87/87 CTest、Release/headless + update 83/83 CTest、Python 工具 115/115、十一份显式手写源码 Cppcheck 均通过。证据位于 evidence/adaptation/，前缀为 demo-pdo、pdo-release、pdo-python-retry 和 pdo-analysis。首次 Release 构建暴露解释器查找晚于身份生成目标；小范围 CMake 修复在构造命令前解析 Python，也覆盖非测试构建。首次 Python 失败还暴露新增 HIL 清单数量及本地 inspector/工具路径缺失；清单现包含第十项 PDO 测试，完整重跑使用既有真实 inspector/cpio/mkenvimage。未降低任何门槛。固件与实物证据另行记录。

### Demo PDO 实板结果

候选 `demo-pdo-20260929` 由 framework 495175f 构建，并通过现有 CAN OTA 路径安装。包 SHA256 为 `65e1e55b9ae3c29f91ec9f58a567e0db6f93657e29745f0ff46554fcaf06b45c`；目标 OS 镜像 SHA256 为 `30b952a1aa9e4098f619dbf89828245bdf0d7e75702f58d24d8a937d50fd22ca`。COM11 和 PCAN_USBBUS1 重启身份为 product `reference-demo`、board `reference-board`、固件 `demo-pdo-20260929`、platform `495175f`。实物 HIL 10/10 通过，包含 `test_demo_instrument_pdo`；XML、UART、CAN ASC 和逐项采集位于 `evidence/ota/demo-pdo-20260929/`。构建报告记录 SDK 固定到 6560f24d 和配置恢复。

Host 截图显示黑底 800x480 主界面，中文字体和绿色/红色状态对比已放大，底部导航分离。它仍是 Host 渲染证据；HIL 不代表触控手感和目标屏视觉验收。

### 密码页布局修正

Demo 密码编辑页参考 参考项目 参考产品页面，在现有 800x480 外壳内调整为：左侧系统设置/高级设置双项菜单，右侧标题和返回键，宽四位掩码输入框，以及四行三列数字键盘。键盘改为 Product UI 自有 button matrix，明确处理数字、退格和确认，不再依赖 LVGL 键盘焦点。保留 Demo 黑色主题，同时恢复参考页面的比例和间距。`ui-pagination` 生成 `password-page.bmp` 并继续验证用户/管理员权限路径；针对 Demo 的 UI 测试已通过。目标屏视觉验收仍需单独 OTA 验证。


密码页 OTA 结果：`demo-password-reference-20260929` 由 framework 341f7c3 构建，包 SHA256 为 `2f7020107c5c2e48aa4b9adb9cd3326c656fbec28da2a393c0026810ae572a10`，已安装并重启验证通过。UART/CAN 报告 platform 341f7c3、board reference-board、LVGL 9.6.0。验证后已释放板卡会话。

### Demo UI 视觉重设计

视觉复查发现，上一版配色和导航让 Demo 更像通用软件仪表板。本批次为主界面、监控、故障、设置和密码页建立统一的仪表视觉：近黑色画布、克制的石墨色面板、白色和灰色文字层级、橙色选中/操作强调色，并将绿色/红色保留给车辆状态。底部导航改为小图标加真实翻译标签，内容区和翻页控件保持已有几何与分页契约。密码页侧栏与主导航使用同一橙色选中态。本批次未改变协议、快照、权限或时序行为。

新的 SDL 截图位于 `evidence/adaptation/redesign-captures-3/`。主题和导航调整后，`ui-pagination`、`demo-settings-app`、`i18n-ui` 和 `sdl-smoke` 均通过。这些是主机视觉证据；目标屏幕配色、触摸手感和实板渲染验收留待下一次明确安排的 OTA 批次。

实板验证 2026-09-29：候选 `demo-ui-redesign-20260929` 由 Framework `d63793e` 和 SDK `967d9ec0` 构建。OS 镜像 SHA256 为 `a43cdc7b61c819f740c3f997e762878ad8125ca3be5c03648d64b79777f7df1f`，CAN OTA 包 SHA256 为 `35aff422d7d38031a342de555488098633b82484169b40d55e82cc5e1ac6c15d`。主机包预检通过，开发板接收 1,106,944 字节，激活并重启完成；重启后的探针报告 `reference-demo/reference-board`、版本 `demo-ui-redesign-20260929`、platform `d63793e`、SDK `967d9ec0`、状态 `IDLE`、错误 `0`、队列拒绝 `0`。原始探针/下载/激活/验证证据位于 `evidence/ota/demo-ui-redesign-20260929/`。这证明固件组合和 OTA 身份正确；目标屏幕的视觉验收仍需面板照片或屏幕采集。

### Demo UI 密度细化

本轮视觉复查还发现两个问题：翻页控件占用空间过大，设置页仍像互不相关的控件排列。现在翻页显示为紧凑的 `< 1/4 >` 形式，同时左右保留独立的 64 x 44 触摸热区。设置页改为固定左侧分类栏和单一右侧内容面板，较长的英文分类名称会在触摸区域内换行。主界面移除装饰性圆角卡片和直接分区标题，并放大五个叉车状态图标。页面边界、翻译、动作和快照语义保持不变。新的 SDL 截图位于 `evidence/adaptation/ui-refine-captures/`。

实板验证 2026-09-29：候选 `demo-ui-refine-20260929` 由 Framework `8d3a494` 和 SDK `fd484e09` 构建。OS 镜像 SHA256 为 `414e3609ba63504297f84e18a0c29afe92bafcf8977fa9253ef810e2a624d941`，CAN OTA 包 SHA256 为 `cd9aa864476c40663090d87a3a423349ce29d4ce9530f3074d728d307e0e6b7f`。主机预检通过，开发板接收 1,106,944 字节，激活并重启完成；重启后的探针报告版本 `demo-ui-refine-20260929`、platform `8d3a494`、SDK `fd484e09`、状态 `IDLE`、错误 `0`、队列拒绝 `0`。原始证据位于 `evidence/ota/demo-ui-refine-20260929/`。本次未采集目标屏幕照片，因此视觉验收仍以主机截图和板端身份/启动验证为边界。

### Demo 仪表底栏与计数显示

主界面底部现在由四个等宽 200 px 触摸 Tab 铺满 800 px 画布，去掉原先的圆角间隙；每个 55 px 触摸区内的图标和真实翻译标签保持居中。主界面新增 Demo 里程和工作小时计，并与车辆状态带并列显示。里程明确是合成的参考值；小时计保留快照值和有效性状态，包括有效零值、过期和错误。五个车辆状态改为纯图标，并将源图标放大 2 倍；绿色、红色和警告色仍分别表达激活、未激活和不可用状态。

确定性 presentation 和翻页测试覆盖计数器有效性、连续导航边界、纯图标标签、放大图标及底栏几何。主机截图位于 `evidence/adaptation/ui-mileage-captures/`；Debug 全量测试 87/87 通过。本批次尚未作为目标屏幕视觉结果验收，下一次 OTA 的身份和面板采集需单独记录。

### Demo 仪表底栏 OTA 结果

候选 `demo-ui-footer-20260929` 由 Framework `ca5a02b` 和 SDK `b082da95` 构建。OS 镜像 SHA256 为 `b97a3e575db697646c1d4d3f25eb10100435f78c3737ce91d54ce7e92e44f067`；CAN OTA 包 SHA256 为 `3f914781193537a4b140358eda672ad8f62c0a6b231a82786adc61469fe1b5af`。主机预检通过；在 COM11/PCAN_USBBUS1 上完成维护准入、下载、激活和重启，开发板接收 1,106,944 字节且队列拒绝为零。重启探针报告 `reference-demo/reference-board`、版本 `demo-ui-footer-20260929`、platform `ca5a02bd940b3a5575d29796323044bafc3a9b57`、SDK `b082da95f8bdd646951d38badf22117e961922f6`、状态 `IDLE`、错误 `0`。原始证据位于 `evidence/ota/demo-ui-footer-20260929/`。本次未采集目标屏幕照片，因此视觉验收仍以主机截图和板端身份/启动验证为边界。

### Demo 设置页紧凑化

设置页现在更接近参考叉车仪表布局：保留一个标题、四项分类栏和一个聚焦内容面板。移除说明性副标题、Demo 凭据/会话标题以及管理员长锁定提示。用户设置只保留四个可操作行；密码页保留用户/管理员入口、登录状态和退出登录；管理员授权前使用紧凑警示图标，授权后显示设置项。现有权限、本地 Settings 所有权和控制器 Parameter 行为不变。

主机截图位于 `evidence/adaptation/ui-settings-compact-3/`。四个设置页和密码流程继续由 `ui-pagination`、`demo-settings-app`、`i18n-ui` 和 `sdl-smoke` 覆盖；Debug 全量测试 87/87 通过。本批次不宣称目标屏幕视觉结果。

### Demo 设置页紧凑化 OTA 结果

候选 `demo-ui-settings-20260929` 由 Framework `09c50cd` 和 SDK `9187a19d` 构建。OS 镜像 SHA256 为 `8255dce099b1ab912fb3dd205d3250007fdd51d1e62b6af7ac8fc687a56bdcea`；CAN OTA 包 SHA256 为 `ee1a54e1ece8bc84b7105a3f26c87c004c53ddf366f0964e0ac05cf078cab8b6`。主机预检通过；在 COM11/PCAN_USBBUS1 上完成维护准入、下载、激活和重启，开发板接收 1,106,944 字节且队列拒绝为零。重启探针报告 `reference-demo/reference-board`、版本 `demo-ui-settings-20260929`、platform `09c50cd188ade20b27ef6921c49f1f8ed4ccdc02`、SDK `9187a19df878ce1ebeb1f4af05992290b1871411`、状态 `IDLE`、错误 `0`。原始证据位于 `evidence/ota/demo-ui-settings-20260929/`。本次未采集目标屏幕照片。

### 监控页与设置页统一布局

Demo 监控页现在与设置页采用相同组成：左侧垂直分类栏，右侧单列条目面板。参数监控和控制器参数设置通过左侧两个居中的 Tab 切换；监控读数和可编辑值在右侧按标签/数值对齐显示。监控条目密度已按完整目录调整，但页数、权限、快照和事务行为不变。确定性 UI 测试在双语场景下检查 Tab 与右侧面板几何。截图位于 `evidence/adaptation/ui-monitor-settings-unified/`。

### 监控页每页四项

监控遥测和故障列表现在每页显示四个条目，与设置页每页四项保持一致。监控目录按四行分页，控制器参数编辑仍使用右侧单页面板。页数和边界按钮根据目录数量确定，信号、权限和事务契约不变。

### 左侧 Tab 全宽与排版细化

监控页和设置页的左侧栏现在贴靠页面左边缘，组成连续无缝的 200 px 栏，Tab 填满内容高度并取消圆角间隙。右侧监控条目增大标签/数值字体并扩大行间距；每页四项规则继续由测试强制检查。完整 SDL 截图（主界面、监控、故障、设置和密码页）位于 `evidence/adaptation/ui-left-tab-full/`。

### 左侧 Tab 全宽 OTA 尝试

主机 SDL 验证通过，完整截图位于 `evidence/adaptation/ui-left-tab-full/`。候选 `demo-ui-lefttab-20260929` 由 Framework `31d8ac6` 构建；OS 镜像 SHA256 为 `e3746fe21833217bbe6e1c6ac3b778e84075b6a96970694838b27c63e4869375`，OTA 包 SHA256 为 `ae0e3736862fad99989f40d1cf7c64502464afde335a2af99c4014207a15a011`。主机包预检通过。开发板接受清单和首个 512 字节块后，在下一次传输返回 UDS `RequestOutOfRange (0x31)`。已执行中止、维护开关重置和开发板重启后重试，仍得到相同响应。原始尝试位于 `evidence/ota/demo-ui-lefttab-20260929/`。开发板仍运行 `demo-ui-settings-20260929`，本次不宣称 OTA 成功。

### 左侧 Tab 紧凑视觉细化

监控页和设置页的分类控件继续贴靠左边缘，并统一使用连续的 64 px 触摸区。设置页左侧栏改为透明容器，只由四个按钮表达选中或未选中状态，避免左侧形成过大的背景区域。监控页和设置页仍共享相同几何、字体、真实翻译及每页四项规则。新的 SDL 截图位于 `evidence/adaptation/ui-left-tab-compact/`；定向 UI 测试通过，全量测试基线仍为 87/87。

### 分类菜单统一样式

监控页和设置页现在共用 `demo_theme_menu_button` 分类控件规则：页面分别维护 200 x 64 px 几何，圆角、边框、文字颜色、选中橙色和按下反馈统一由主题提供。设置页不再直接覆盖菜单颜色，因此两个左侧菜单具有一致状态表现，也不会增加整栏背景。现有导航和权限行为不变。

### 条目与密码控件统一

设置页右侧条目、分类菜单和密码输入页现在共用 Demo 主题的深色表面、橙色选中状态、弱化标签、边框处理和间距节奏。密码输入改用同一深色输入框和主题键盘，不再使用独立的白色控件；密码页左侧选项也使用统一分类按钮规则。权限行为和键盘事件处理不变。紧凑 SDL 截图已在修改后重新生成。

### 右侧扁平条目列表

右侧监控、控制器参数和设置条目不再使用嵌套圆角矩形。条目表面透明，仅保留一条底部分隔线，深色画布和每页四项节奏更加清晰。翻译文本超过触摸区时，分类标签使用单行循环滚动。左侧四个设置分类继续通过菜单进入独立页面，权限和编辑器行为不变。SDL 截图已重新生成于 `evidence/adaptation/ui-left-tab-compact/`。

### 设置路由与状态显示

Demo 现在提供两个设置路由：用户设置和管理员设置。用户设置中包含可点击的仪表版本条目，点击后进入版本详情页，并通过返回控件回到用户设置。未授权时点击管理员设置会先打开管理员密码输入页，只有现有权限结果允许后才显示管理员页面。原独立密码 Tab 已移除。设置页和监控页左侧 Tab 区域使用铺满背景，监控数值和标签改为白色以提高对比度。路由截图位于 `evidence/adaptation/ui-routing-admin-version/`。

### 管理员/版本路由 OTA 边界

两路由设置改动已通过主机编译和全量 87 项测试。已尝试构建 `demo-ui-routing-20260929b`，SDK 封装正确恢复 `.config`，但 SCons 在生成镜像前因 mkimage 配置缺少 cluster size（`int(None)`）停止，所选环境也无法解析 SDK Python 辅助工具。未生成 OTA 包，开发板状态未改变。该问题属于构建环境阻塞；此前板端 `0x31` 传输证据保持不变。

### 设置页布局修正与 OTA 诊断

路由截图暴露了两个布局问题：菜单文字在宽触摸区内左对齐，仪表版本条目与最后一行用户设置重叠。现在菜单文字居中，用户设置五个条目采用紧凑纵向节奏，版本条目保持在内容面板内部。监控读数标签和数值继续使用白色。OTA 失败与本次 UI 改动无关：SDK 封装已进入 SCons，但当前 SDK `.config` 缺少 mkimage 的 cluster-size 配置，且辅助 Python 别名无法解析，因此未生成镜像。此前成功 OTA 使用的是完整板级配置，重试前应恢复该目标配置。

待决策项：本次布局修正没有启用 CAN 波特率持久化。当前 Demo CAN 波特率条目仍是内存中的管理员偏好；要在重启后生效，需要扩展保留设置格式并定义启动时 CAN 初始化契约。该项作为有界后续任务保留，避免静默改变持久化格式。

### 用户设置页每页五项

用户设置内容面板现在在保持原字体大小的前提下显示五个紧凑条目：速度单位、语言、显示亮度、演示速度上限和仪表版本。五行均在面板内完整排布且不重叠；仪表版本条目仍可进入独立详情页。管理员路由和现有 CAN 波特率条目保持不变，等待保留设置扩展决策。

### 恢复 SDK Python/SCons 构建环境

板级封装现在使用 SDK 内置的 Python 3.8 可执行文件 `tools/env/tools/Python38/python3.exe` 和内置 SCons 3.1.2 库。Python 2.7 仅保留给旧 SDK 工具使用，当前应用 SConscript 依赖 `importlib.util`，不能用 Python 2.7 构建。候选 `demo-ui-routing-20260929` 已用该环境重新构建成功，生成 D50T-2-Lite 镜像和 OS ITB，并逐字节恢复 `.config`。证据位于 `evidence/ota/demo-ui-routing-20260929e/`。

### Demo 设置路由 OTA 结果

候选 `demo-ui-routing-20260929` 使用恢复后的 SDK Python 3.8/SCons 环境重新构建，并通过主机完整性预检。OS 镜像 SHA256 为 `ae80d1cedd4969e1086cfaa2ff722a7ecd0a5c6bd87c04a956f64d3b5e60d0ad`；OTA 包 SHA256 为 `297439da36e89c2d872ca72993a0fc49f504e0759c5f0f4aa2fdc723e1c2c1c3`。在 COM11/PCAN_USBBUS1 上完成维护准入、接收 1,111,040 字节、激活和重启。重启后身份为 `reference-demo/reference-board`，固件 `demo-ui-routing-20260929`，Framework `d55321b`，SDK `9db13a6c`，LVGL `9.6.0`，状态 `IDLE`，错误 `0`，队列拒绝 `0`。原始证据位于 `evidence/ota/demo-ui-routing-20260929/`。

### Tab 内翻页与统一字体排版

翻页控件现在位于当前右侧内容面板左上角，底部导航只负责页面路由。监控条目从翻页控件下方开始，采用统一的每页五行排版目标；故障条目继续使用自己的 Tab 内翻页。设置标题和用户设置条目改用与监控面板一致的较大字体和间距节奏。现有仪表版本详情路由保留返回控件。


### 每页五行目标与独立设置详情

Demo 监控、故障和设置内容统一采用每页五行的排版目标：条目高度 56 px、行间节奏 60 px，并使用底部分隔线。监控和故障翻页仍只作用于当前 Tab，紧凑翻页控件固定在右侧内容区上方靠右位置。用户设置保留五个外部可见条目，速度单位、语言、亮度和速度上限分别进入独立详情页，并通过返回键回到列表。列表只呈现当前快照，进入详情不会发送动作；管理员条目在授权后使用相同条目几何和详情面板。主机 SDL 截图和确定性路由/动作测试位于 `evidence/adaptation/ui-five-row-details/`。本批次不启用 CAN 波特率持久化，也不改变任何保留格式。
