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
| H-05 | 记录原生共享状态/锁所有权，逐步增加启停故障注入接口（M-01/M-05），不移动 worker 或改变同步。 | 测试覆盖已初始化资源和确认、队列拒绝及耐久阻塞；区分确定性替身与目标结果。 | 待执行 |
| H-06 | 清点分析器覆盖，按可评审分组扩展手写第一方范围（M-06）；验证文档链接和生成器失败路径（M-09）。 | 范围工件核对选中源码及编译命令；分析发现修复或明确待人工评审。 | 待执行 |

H-01/H-02 保留下方原始验证记录。2026-09-28 续接工作使用外部真实产品需求检验这些优先级。每批先读相关契约、编辑前建立特征测试、保留外部语义、运行直接相关测试及选定 Product 矩阵并记录限制。避免大范围格式变动或新增抽象层。

## 需求驱动的续接工作

公共仓库只保存可复用结论。私有需求文档已作为证据阅读，不复制到源码、目录或样例。本地忽略的证据记录保留文档哈希和待定 ID。所有 P0 仍未解决：A1/A2/A3、B1/B2/B3/B4、C1/C2、D4、E1、F3 和 G1。条目内建议不代表批准。总线分配、标定前提、计数策略及默认值即使被来源标为 P1 也仍需确认。本次不引入真实产品协议、密码、故障表或配置。

| 能力 | 当前实现 / 实际缺口 | 下一步复用工作 |
|---|---|---|
| Product 组合 | 宿主清单可选择 Product；固件启动/存储/翻译仍绑定 Demo。 | D-02：评审 Product 自有启动及选中源码闭包，不静默改变固件契约。 |
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
| D-02 固件 Product 选择 | 为 Product 存储/UI/协议源码设计一个明确组合边界；实施前评估两个目标 Product。 | 集成者/维护者；未决 |
| D-03 生命周期失败 | 改变所有权或停止保证前决定清理/重启/耐久失败语义。H-05 可独立描述现有行为。 | 维护者；未决 |
| D-05 持久化迁移与计数语义 | 保留未知模式不覆盖及双槽恢复；评审已知旧模式迁移、计数单位/使能/复位/检查点和掉电预算。未确认映射与默认值不得写入。 | 产品/维护者；未决 |
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
