# 生产运行时

## 当前架构

固件采用 Protocol、App/Core、CAN TX、LVGL UI、NVM 和 Update 独立 owner，使用 RT-Thread 原生有界 IPC。旧单线程 CAN/UI smoke 及 OTA 独立 RX/TX 分支已被统一执行端口替代。现行实现不会在 UI 链路中排空 CAN，也不在发布锁内执行驱动或 LVGL。

## 边界

Core 和公共运行时不依赖 OS/LVGL。Protocol 拥有解码和协议状态；App 独占 Core 和产品 workflow。只有 TX worker 可调用 CAN 驱动写接口。UI 接收数组副本并提交意图副本。有界原生队列过载时拒绝完整消息，不引入万能 Event Bus。IPC 和 worker 就绪后才打开 CAN。停止须等待在途 I/O；超时报告停止未完成，绝不销毁活跃 worker。

## 验收状态

当前 Host、Linux 分析与单总线实板验证见[验证记录](validation.zh-CN.md)。记录绑定原始 firmware SHA 和镜像 hash，历史整理不改变板上固件身份。双总线实板仍未验证；不宣称正式 MISRA 合规、rollback 或掉电恢复通过。

## 执行与所有权

| Owner | 触发 / 阻塞边界 | 静态栈 |
|---|---|---|
| Protocol 优先级 19 | RX 信号量、每轮最多 64 帧、总线公平轮询；UDS 1 ms 或普通 5 ms 等待；不调用 driver write | 8192 字节 |
| App 优先级 20 | 批次/事件/动作唤醒或 5 ms tick；不执行设备/Flash/LVGL | 8192 字节 |
| CAN TX 优先级 18 | 每总线唯一 writer，队列唤醒，空闲无限等待；阻塞 driver write | 每总线 2048 字节 |
| UI 配置优先级 + 2 | 普通 16 ms / 维护 50 ms 执行 LVGL | 默认 32768 字节 |
| NVM 优先级 21 | 现有队列；阻塞 EEPROM I/O | 现有 NVM 预算 |
| Update 优先级 25 | 现有队列；Flash 及 durable barrier 等待 | 12288 字节 |

Core 和 Product 工作流仅由 App 执行。Reference-Mixed 启动同步使用语义命令及已解码参数事件，不持有 CANopenNode 通道。不可变 Product getter 不再复位协议状态。Host 测试分步驱动 App/Protocol，不模拟原生调度器。

## 端口与生命周期

Protocol 每次解码最多暂存 32 个值，整批复制到 64 个原生队列槽位之一。App 在写入前完成全部校验与 source arbitration。队列满或超容量时整批拒绝。事件使用 16 个复制槽位；旧 generation 批次/事件丢弃。Product 回调借用值仅在调用期间有效。

UI 只有一个请求信用和一个保留结果。成功提交表示 QUEUED，App 报告 APPLIED/REJECTED。Core、UI 发布区、诊断发布区、UI 本地副本互相独立并拒绝别名。UI 不保存 Core 数组。

App 只有一个保留命令/结果信用，身份由 generation 和不复用的 64 位序号组成。QUEUED、APPLIED、TX_COMPLETED、REMOTE_CONFIRMED 互不等价。同步命令帧携带身份，驱动完成后才报告 TX_COMPLETED。异步 SDO 由真实响应报告 REMOTE_CONFIRMED，不伪造无法关联的发送完成。超时/取消经 adapter cancel/reset 停止本机重试。已送达的远端写入可能已生效，本地取消不保证回滚；Product 重试非幂等命令前必须核对不确定的远端状态。

每总线有 16 个普通和 16 个紧急 TX 槽位，最多连续四个紧急帧后给普通帧一次机会。旧 generation 帧丢弃。入队不证明线上送达。不引入万能 Event Bus 或跨 owner 临时指针。

## 模式与期限

App 独占 STARTUP、NORMAL、DEGRADED、UPDATE_MAINTENANCE、SHUTDOWN，Product 提供能力矩阵。原生队列满计数器（`batch_full`、`event_full`、`tx_full`）增加时，在未选择维护模式的情况下选择 DEGRADED；连续一秒无新增队列拒绝可恢复。语义批次校验和 TX 发布失败增加 `batch_rejected`，但它不参与过载计算。改变此边界需按[维护者评估](maintainability.zh-CN.md) 中 D-01 进行产品策略评审。这是参考策略而非安全保证。模式变化推进 generation、将旧 Domain 信号标 stale、复位协议会话和产品工作流。OTA 返回普通模式需结束显式维护请求；Abort 不覆盖操作者的维护选择。

Reference-Demo 在 CAN0 每 50 ms 发送合成 0x3C0，每 100 ms 发送 0x2F0。维护模式仅保留关键 0x3C0。它们是公开台架数据，不是车辆控制。期限从上次计划值推进；迟到时跳过错过周期，不补发突发。模式切换显式重置相位。Mixed TPDO 也使用该 helper。线上间隔仍受队列和驱动延迟影响，必须 PCAN 实测。

动态值、sample/publish time、semantic revision、generation、pending 替换和到期丢弃见[Dynamic TX 契约](dynamic-periodic-tx.zh-CN.md)。这些周期消息使用独立有界 mailbox，不在普通 TX 队列积累历史值。

## 生命周期与诊断

INIT 初始化静态 IPC 和快照。App 启动 NVM/Update 及 TX 后，Protocol 才打开 CAN。全部配置总线打开后 READY 转 RUNNING。设置等待 NVM restore 完成。启动失败进入 FAILED 后协作式 STOPPING。

meter_exec stop 拒绝新业务/TX、取消排队工作、停止 Protocol 处理、请求 Update 退出、将 NVM flush 到 durable revision。在途阻塞 I/O 自然返回。App 等全部 owner 确认后解绑 RX、关闭 CAN，UI 最后释放 LVGL。STOPPING 超五秒报告 overdue，不强杀或释放 worker。STOPPED 后需重启，静态 IPC 保留到重启。durability barrier 失败时故意不宣称 STOPPED。

Owner 在短锁中复制诊断，MSH 锁外格式化。设备/Flash/LVGL 不持发布锁。Trace 每次锁内最多复制 16 项。meter can 包含原生 FIFO 丢包。meter_exec 展示模式/generation、队列水位/拒绝、runnable 次数、计划错过和停止等待。原生 list_thread 提供栈水位。空闲阻塞属于健康等待；执行次数是观测而非 watchdog 判断。

## 质量与门禁

逐步启用 portable runtime 严格告警。Linux CI 配置 cppcheck、clang-tidy analyzer/bugprone/cert/performance、ASan/UBSan、有界 libFuzzer，覆盖 record/settings/package parser/request ledger/protocol decode。第三方 adopted 源码保持不变。源码所有权门禁仅防回归，不构成无数据竞争证明。

Windows Host、Target build、实板 HIL 分别报告并绑定 SHA 证据。配置 Linux 分析不代表已运行。多 bus HIL 需要两个物理接口和匹配 Product 固件。Target 告警覆盖、真实调度压力、长时间时序、各 backend 操作中的停机仍是待实测的生产门禁。不声明正式 MISRA、AUTOSAR 一致性、rollback 或断电恢复。

Product 可声明命令完成于 APPLIED、TX_COMPLETED 或 REMOTE_CONFIRMED，默认要求远端响应。仅发送命令保留 TX_COMPLETED 终态，不会随后被误报为超时。采用 App 会话代次时复位 Adapter，并立即同步诊断代次，维护期间暂停遥测也不会显示旧代次。

分析器的真实覆盖范围、Annex K TAD-001 待人工批准状态及 required checks 缺口统一维护在[治理状态](compliance/status.zh-CN.md)，不将 advisory 例外视作已批准的 MISRA deviation。
