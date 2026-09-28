# 工程治理基线

## 状态及强制执行

基线 ac3dc01，检查日期 2026-09-28。独立区分 Tool Quality Green、Project Governance Conforming、Formal MISRA Compliance，目标为前两层。本项目采用 MISRA C:2025-ready 第一方工程流程，不宣称正式合规。没有授权规则原文或正式检查器，不复制专有规则。

Tool Quality Green：基线 GitHub run 36419980592 attempt 2 的 host 和 quality 通过，后续修改重新验证。Project Governance Conforming：人工适用性评审及 required checks 强制执行待闭合。Formal MISRA Compliance：未评估。

实际 main 保护查询返回 HTTP 404（Branch not protected），仓库 rulesets 为空。这些 CI job 当前不是受保护的 required checks，管理员配置后才能宣称存在强制执行。

## 范围及实际分析覆盖

第一方生产范围包含 contracts、core、runtime、storage、update、protocols/common、自主 platform/rtthread 和 Product C。DBC/catalog 生成 C 仍属生产代码，保留生成器版本、输入及输出证据。字体/二进制资源独立归类。

当前 Cppcheck、clang-tidy 仅枚举 core、runtime、storage、update；头文件过滤还含 contracts。Host CMake 命令不代表 target 覆盖。这些 job 未全面分析 Product、generated、platform、UI 翻译单元。编译器、生成器、架构、public-clean 提供不同证据，不代表完整 MISRA 覆盖。

采用的 RT-Thread/ArtInChip SDK、LVGL、CANopenNode、iso14229 保持 upstream ownership。依赖/构建证据记录固定 SHA、许可、使用边界、已知 finding 和验证；缺失证据保持未闭合，不为第一方工具变绿修改 upstream。

## 项目规则

每条规则都需受影响范围的人工评审和精确范围例外记录，自动检查只是部分证据。

| 规则 | 意图 / 范围 | 自动执行 | 人工评审 |
|---|---|---|---|
| FMP-C-001 | 单一可变 owner | Ownership 门禁 | 状态、IPC 复制、生命周期 |
| FMP-C-002 | 不用 volatile 同步，不跨 owner 借可变指针 | 架构/编译器 | 锁及生命周期证明 |
| FMP-C-003 | 校验外部指针、范围、非法状态 | Analyzer/sanitizer/负测 | 外部边界审查 |
| FMP-C-004 | 资源及运行期堆有界 | 架构门禁 | 容量、ISR 边界、阻塞、栈 |
| FMP-C-005 | 处理关键失败及返回 | 编译器/analyzer/故障测试 | Admission 与 completion |
| FMP-C-006 | 内存/字符串操作有界 | Analyzer/sanitizer | 长度及重叠证明 |
| FMP-C-007 | Core/runtime 可移植，owner 独占 I/O | 架构/ownership | 无 OS/LVGL/设备越界依赖 |
| FMP-C-008 | 单调回绕安全时间 | Deadline 测试 | 半周期及过期证明 |

## 例外及 TAD-001

记录 ID、规则/工具、精确位置、原因、风险、替代方案、验证、owner、审批状态及复审条件。先尝试修复代码。Agent 不能批准例外，门禁配置变化必须显式记录。保留告警、分析范围、sanitizer、fuzz 边界、HIL 门槛，不删测试、弱化 error、排除第一方文件或错误分类代码来通过。

TAD-001 属于 Tool Applicability Decision，不是 MISRA deviation。范围为 .clang-tidy 既有 WarningsAsErrors 对 clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling 的例外。检查器告警保持启用且可见。Owner：项目维护者。审批：PENDING 人工评审。原因：未确认 target libc 的 Annex K 替换可用性。风险：checker 还覆盖其他需逐处评审的 API，全局严重度例外不能证明安全。替代方案：审计 libc 支持或按评审位置缩小范围。证据：4e31062/ac3dc01 的有界复制/空值/几何修复及基线 quality run。libc、checker、工具链或调用位置变化时复审。本次没有新增 suppression。

## 工具链证据

每次构建记录实际 target GCC/Xuantie、Host GCC、Clang、clang-tidy、Cppcheck、Python 版本。ASan、UBSan、libFuzzer 是编译器运行时能力，不是正式 MISRA 检查器。记录实际 SCons target 的 defines、includes、march、mabi、优化及语言模式，Host 编译数据库不足以替代。版本漂移或 target 分析缺口在交付报告中保持可见。
