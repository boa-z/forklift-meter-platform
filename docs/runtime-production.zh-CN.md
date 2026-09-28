# 生产运行时

## 决策与基线

基线：`3b8943a`。UI 循环每 16 ms 加渲染耗时最多消费八帧普通 CAN。OTA 增加独立原始 RX owner 和 64 帧转发队列，但普通解码仍等待 UI。这限制持续排空能力，单纯扩大原生 FIFO 无法关闭 burst 门禁。本任务不修改 HIL 门槛。

已有 batch builder、深复制快照、request ledger 和单调时钟助手属于可移植契约，固件尚未使用。NVM 已将 App 所有的 revision/barrier 与阻塞 EEPROM I/O 分离。OTA 已分离升级 job 与 Flash，但另有 Protocol/TX 实现。当前诊断大锁覆盖 Core、LVGL 与驱动调用。

## 实施顺序

1. 使用 designated initializer 修复 Reference 产品可选字段回归。
2. 增加可测试生命周期、Product 模式策略和计划周期期限；将完整语义批次接入运行时解码。
3. 统一固件 Protocol runnable、App 单写者、UI 快照消费者和 CAN TX worker；复用原生 RT-Thread IPC 与 NVM/Update worker。
4. owner 发布诊断，复制锁内禁止格式化和设备 I/O；实现协作停止与旧会话拒绝。
5. 执行 Host 产品/OTA 门禁并添加静态分析、sanitizer 和 fuzz 基线；构建候选镜像，不修改 SDK 源码或配置。
6. 对具体镜像实测 CAN 时序、burst、NVM 与 OTA。固件编译不能关闭这些门禁。

## 边界

Core 和公共运行时不依赖 OS/LVGL。Protocol 拥有解码和协议状态；App 独占 Core 和产品 workflow。只有 TX worker 可调用 CAN 驱动写接口。UI 接收数组副本并提交意图副本。有界原生队列过载时拒绝完整消息，不引入万能 Event Bus。IPC 和 worker 就绪后才打开 CAN。停止须等待在途 I/O；超时报告停止未完成，绝不销毁活跃 worker。

## 验收状态

已实现，并在固件 368eed1 上执行验证。Host 产品矩阵、八项实板 HIL、CAN OTA 激活、NVM 共存与协作停机通过所记录的检查。具体哈希、测量及剩余门禁见 [Runtime 重构报告](runtime-refactor-report.zh-CN.md)。多总线实板与 Linux 插桩分析仍未验证；不宣称正式 MISRA 合规、rollback 或掉电恢复通过。

## 执行与所有权

| Owner | 触发 / 阻塞边界 | 静态栈 |
|---|---|---|
| Protocol 优先级 19 | RX 信号量、每轮最多 64 帧、总线公平轮询；UDS 1 ms 或普通 5 ms 等待；不调用 driver write | 8192 字节 |
| App 优先级 20 | 批次/事件/动作唤醒或 5 ms tick；不执行设备/Flash/LVGL | 8192 字节 |
| CAN TX 优先级 18 | 每总线唯一 writer，队列唤醒，空闲无限等待；阻塞 driver write | 每总线 2048 字节 |
| UI 配置优先级 + 2 | 普通 16 ms / 维护 50 ms 执行 LVGL | 默认 32768 字节 |
| NVM 优先级 24 | 现有队列；阻塞 EEPROM I/O | 现有 NVM 预算 |
| Update 优先级 25 | 现有队列；Flash 及 durable barrier 等待 | 12288 字节 |

Core 和 Product 工作流仅由 App 执行。Reference-Mixed 启动同步使用语义命令及已解码参数事件，不持有 CANopenNode 通道。不可变 Product getter 不再复位协议状态。Host 测试分步驱动 App/Protocol，不模拟原生调度器。

## 端口与生命周期

Protocol 每次解码最多暂存 32 个值，整批复制到 64 个原生队列槽位之一。App 在写入前完成全部校验与 source arbitration。队列满或超容量时整批拒绝。事件使用 16 个复制槽位；旧 generation 批次/事件丢弃。Product 回调借用值仅在调用期间有效。

UI 只有一个请求信用和一个保留结果。成功提交表示 QUEUED，App 报告 APPLIED/REJECTED。Core、UI 发布区、诊断发布区、UI 本地副本互相独立并拒绝别名。UI 不保存 Core 数组。

App 只有一个保留命令/结果信用，身份由 generation 和不复用的 64 位序号组成。QUEUED、APPLIED、TX_COMPLETED、REMOTE_CONFIRMED 互不等价。同步命令帧携带身份，驱动完成后才报告 TX_COMPLETED。异步 SDO 由真实响应报告 REMOTE_CONFIRMED，不伪造无法关联的发送完成。超时/取消经 adapter cancel/reset 停止本机重试。已送达的远端写入可能已生效，本地取消不保证回滚；Product 重试非幂等命令前必须核对不确定的远端状态。

每总线有 16 个普通和 16 个紧急 TX 槽位，最多连续四个紧急帧后给普通帧一次机会。旧 generation 帧丢弃。入队不证明线上送达。不引入万能 Event Bus 或跨 owner 临时指针。

## 模式与期限

App 独占 STARTUP、NORMAL、DEGRADED、UPDATE_MAINTENANCE、SHUTDOWN，Product 提供能力矩阵。批次/事件/TX 拒绝后进入 DEGRADED，连续一秒无新增拒绝可恢复。这是参考策略而非安全保证。模式变化推进 generation、将旧 Domain 信号标 stale、复位协议会话和产品工作流。OTA 返回普通模式需结束显式维护请求；Abort 不覆盖操作者的维护选择。

Reference-Demo 在 CAN0 每 50 ms 发送合成 0x3C0，每 100 ms 发送 0x2F0。维护模式仅保留关键 0x3C0。它们是公开台架数据，不是车辆控制。期限从上次计划值推进；迟到时跳过错过周期，不补发突发。模式切换显式重置相位。Mixed TPDO 也使用该 helper。线上间隔仍受队列和驱动延迟影响，必须 PCAN 实测。

## 生命周期与诊断

INIT 初始化静态 IPC 和快照。App 启动 NVM/Update 及 TX 后，Protocol 才打开 CAN。全部配置总线打开后 READY 转 RUNNING。设置等待 NVM restore 完成。启动失败进入 FAILED 后协作式 STOPPING。

meter_exec stop 拒绝新业务/TX、取消排队工作、停止 Protocol 处理、请求 Update 退出、将 NVM flush 到 durable revision。在途阻塞 I/O 自然返回。App 等全部 owner 确认后解绑 RX、关闭 CAN，UI 最后释放 LVGL。STOPPING 超五秒报告 overdue，不强杀或释放 worker。STOPPED 后需重启，静态 IPC 保留到重启。durability barrier 失败时故意不宣称 STOPPED。

Owner 在短锁中复制诊断，MSH 锁外格式化。设备/Flash/LVGL 不持发布锁。Trace 每次锁内最多复制 16 项。meter can 包含原生 FIFO 丢包。meter_exec 展示模式/generation、队列水位/拒绝、runnable 次数、计划错过和停止等待。原生 list_thread 提供栈水位。空闲阻塞属于健康等待；执行次数是观测而非 watchdog 判断。

## 质量与门禁

逐步启用 portable runtime 严格告警。Linux CI 配置 cppcheck、clang-tidy analyzer/bugprone/cert/performance、ASan/UBSan、有界 libFuzzer，覆盖 record/settings/package parser/request ledger/protocol decode。第三方 adopted 源码保持不变。源码所有权门禁仅防回归，不构成无数据竞争证明。

Windows Host、Target build、实板 HIL 分别报告并绑定 SHA 证据。配置 Linux 分析不代表已运行。多 bus HIL 需要两个物理接口和匹配 Product 固件。Target 告警覆盖、真实调度压力、长时间时序、各 backend 操作中的停机仍是待实测的生产门禁。不声明正式 MISRA、AUTOSAR 一致性、rollback 或断电恢复。

Product 可声明命令完成于 APPLIED、TX_COMPLETED 或 REMOTE_CONFIRMED，默认要求远端响应。仅发送命令保留 TX_COMPLETED 终态，不会随后被误报为超时。采用 App 会话代次时复位 Adapter，并立即同步诊断代次，维护期间暂停遥测也不会显示旧代次。

clang-tidy 错误策略继续将全部 analyzer 检查作为 error，仅 clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling 保持启用并作为可见 advisory。该检查对已有长度限制的 memcpy/memset/snprintf 也推荐 C11 Annex K 替代，而可移植 Host/嵌入式契约不要求 Annex K。此单项适用性例外不关闭空指针、零除、缓冲区或无界 strcpy 诊断。各边界仍须验证长度；不通过手写循环或局部 NOLINT 绕开标准库检查。运行 36418914884 暴露了局部防护不足和无界复制形式：新增 frame/context/slot 明确检查及包身份的有界复制修复这些问题。该次 ASan/UBSan 和 cppcheck 通过，clang-tidy 失败、fuzz 跳过，因此不能视为 quality PASS。
