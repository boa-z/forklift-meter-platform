# 当前维护计划

> [English](maintenance-plan.md)

根据[源码评估](maintainability.zh-CN.md) 于 2026-09-28 建立，基线 `81e5083`。这是当前续接记录；随代码变更同步更新此处的状态与证据，不依赖聊天历史。

## 行为保持加固

| 顺序 / 项目 | 范围与理由 | 完成证据 | 状态 |
|---|---|---|---|
| H-01 | 记录架构及差异；从 AGENTS 和文档索引链接计划。按现有源码修正 M-03、M-11 和已确认的所有者注释。 | 双语门禁、源码评审、策略未改变。 | 完成 |
| H-02 | 使 `runtime/meter_periodic.c` 可局部理解：描述性参数、展开分支、明确阶段注释。为发布拒绝原子性、编码拒绝/身份及身份耗尽增加独立测试。 | 五个新增具名用例在原实现通过；保留已有生命周期断言；完整 Product 矩阵通过；优化对象相同。签名、布局、算术、期限和策略不变。 | 完成 |
| H-03 | 为架构/所有权护栏建立反例（M-07），再增强相对 include 处理。保留所有已有门禁及范围。 | 样例拒绝刻意违反、允许合法 include、扫描锚点缺失时报清楚错误；在 CTest/CI 注册。按治理规则记录门禁变更。 | 下一项 |
| H-04 | 保证 C 测试初始化/检查不因 NDEBUG 消失（M-08）。 | Debug 和 Release 均拒绝故意失败的测试；仅影响测试；所有 Product 矩阵通过。 | 待执行 |
| H-05 | 记录原生共享状态/锁所有权，逐步增加启停故障注入接口（M-01/M-05），不移动 worker 或改变同步。 | 测试覆盖已初始化资源和确认、队列拒绝及耐久阻塞；区分确定性替身与目标结果。 | 待执行 |
| H-06 | 清点分析器覆盖，按可评审分组扩展手写第一方范围（M-06）；验证文档链接和生成器失败路径（M-09）。 | 范围工件核对选中源码及编译命令；分析发现修复或明确待人工评审。 | 待执行 |

每批先读相关契约、编辑前建立特征测试、保留外部语义、运行直接相关测试及选定 Product 矩阵并记录限制。避免大范围格式变动或新增抽象层。

## 架构决策

| 项目 | 建议下一步 | 决策人 / 状态 |
|---|---|---|
| D-01 拒绝策略 | 评审实际计数器，决定语义校验拒绝是否影响模式。决定前只记录现有行为。 | 产品/维护者；未决 |
| D-02 固件 Product 选择 | 为 Product 存储/UI/协议源码设计一个明确组合边界；实施前评估两个目标 Product。 | 集成者/维护者；未决 |
| D-03 生命周期失败 | 改变所有权或停止保证前决定清理/重启/耐久失败语义。H-05 可独立描述现有行为。 | 维护者；未决 |
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
