# Runtime 重构报告

日期：2026-09-28。分支：codex/runtime-production。本报告区分已实现内容与尚未完成的生产验收。

## 基线与最终身份

Baseline SHA：3b8943a。Final firmware source SHA：368eed1c346ec87912d4b2c40d366ca0cd1f915f。后续纯文档提交不改变受测镜像。固件版本：runtime-production-j；Product：reference-demo；硬件兼容键：reference-board。

SDK 基线 SHA：9b78386dcaa54326487c8b208b2b7104d4b56c9b。设备报告的 SDK 身份后缀：dirty-6a08b8bf070c2e33。没有修改 SDK 源码、持久配置或父仓库 gitlink，保留原有父仓库工作区状态。恢复后的配置 SHA256：2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241。

主要提交：4db6340 修复 Reference 产品契约与字体；7cbf5a8 实现 owner 与语义 IPC；338f7f0 引入原生单调时钟与 TX 失败退避；ea048a4 保留 Product 指定的命令完成阶段并统一会话 epoch；368eed1 要求 UI 释放确认后才进入 STOPPED。修改已本地提交，本分支尚未推送或合并。

镜像：evidence/runtime-production/build-j，1798656 字节，SHA256 4512e27875634c2f3bed51e21ee4ccb5f4356e5ae3a4b43aec68cdaf8a0dca40。OTA：evidence/runtime-production/package-j/ota.cpio，1098752 字节，SHA256 ff7dca3f1c23beb0034a04f54844e150e3d65ee64903bf850189f72fe2c981d6。包内包含与构建结果完全一致的 OS 字节及原生 4096 字节对齐填充。构建报告的 hardware_validation=NOT_RUN 是构建当时状态，后续实板结果在下文单独记录。

## 架构与所有权

Architecture：[生产运行时](runtime-production.zh-CN.md) 定义 typed port、组件 owner 和由 RT-Thread 任务承载的 runnable。不引入 ARXML、AUTOSAR OS 或万能 Event Bus。Core/common 不依赖 RTOS/LVGL。

Thread / runnable mapping：Protocol 优先级 19 / 栈 8192 字节，处理原生 RX 信号量、解码、CANopen、UDS 和 deadline；App 优先级 20 / 8192 字节，拥有 Core、工作流、模式和发布；每个 CAN TX 优先级 18 / 2048 字节，拥有阻塞 driver write；UI 优先级 22 / 32768 字节，拥有 LVGL；NVM 优先级 24 / 4096 字节和 Update 优先级 25 / 12288 字节，继续承载阻塞存储/Flash。组件不必各有一个线程。

Ownership：Protocol 暂存语义值和事件，不修改 Core。App 验证并应用数据、仲裁来源、发布复制快照。UI 使用自己的数组并提交意图。Debug/trace 使用短复制锁；MSH 在锁外格式化。空闲 suspend 的 worker 属于正常等待，不能作为丢失心跳判断故障。

Mode model：App 拥有 STARTUP、NORMAL、DEGRADED、UPDATE_MAINTENANCE 和 SHUTDOWN。Product 声明能力。拒绝压力进入 DEGRADED；参考恢复策略要求一秒内无新增拒绝。Generation 切换使旧批次/TX 失效，复位 Protocol 会话与 Product 工作流，并将旧 Domain 值置 stale。Abort 保留明确选择的维护模式，直到 maintenance off。

IPC contracts：64 个复制语义批次槽，每批最多 32 个值；App 在任何写入前验证完整批次。事件有 16 个复制槽。UI 意图/结果和 App 命令/结果各保留一个 credit。QUEUED、APPLIED、TX_COMPLETED、REMOTE_CONFIRMED、FAILED、CANCELLED 含义不同。Product 选择成功终态，默认要求远端确认。取消不能撤销已经送达远端的写入。

Periodic scheduling：原生单调毫秒时钟和计划 deadline 避免执行时间累积漂移。Protocol 每轮最多排空 64 帧，跨总线公平轮询后让出执行；空闲时 UDS 等待 1 ms，普通模式等待 5 ms。过期周期跳过，不突发追赶。模式切换明确重置相位。生产路径已移除 UI 每轮轮询八帧的链路。

TX architecture：每总线 16 个普通槽和 16 个紧急槽，最多四帧紧急报文后给普通报文一次机会。只有本总线 TX worker 调用驱动。入队不代表物理发送完成。失败发送退避从 100 ms 增加至 1000 ms；无 ACK 故障时 SDK HAL 日志仍可能与 UART 输出交错。

NVM integration：App 拥有 revision/request/barrier，原有 worker 拥有 EEPROM I/O 和双槽写入。维护模式拒绝普通设置，仅允许必要的持久化协调。停机拒绝业务请求，中止 Update，等待 NVM durable 和 worker 确认，关闭 CAN、释放 UI 后才报告 STOPPED。不强制删除线程；超时停机保持可观测。STOPPED 后须重启恢复。

OTA integration：CAN -> Protocol/ISO-TP/UDS -> 有界 Update job -> Update worker -> 现有原生 OTA backend -> candidate -> NVM barrier -> 激活/重启。移除 OTA 的重复 RX/TX owner。参考维护策略保留关键 0x3C0，暂停普通 0x2F0/telemetry，保留诊断与升级 UI。Hash 是完整性校验，不是签名。

## 测试与测量

Static analysis：可移植层严格编译警告和 architecture/public-clean/ownership/文档门禁通过。Cppcheck OSS 检查 14 个第一方可移植源文件，无发现。Linux clang-tidy、ASan/UBSan、有界 libFuzzer 已配置，但本次 NOT_RUN；fuzz harness 严格语法编译通过。不宣称正式 MISRA 或 AUTOSAR 合规。原生 IPC stub 测试验证编排，不证明真实线程调度或无数据竞争。

Host tests：Demo UI 42/42；Reference-B UI 32/32；Reference-Mixed UI 31/31；可选 OTA/headless 40/40。Python 96 passed、11 skipped（八项显式启用的实板测试和三项环境相关包测试）。日志位于 evidence/runtime-production/*test-final.log 和 python-final.log。各矩阵共享部分门禁，不能解读成 145 项不同测试。

HIL：最终固件 j，evidence/runtime-production/hil-j/20260928T114317Z-92e8e02d，八项测试在 29.43 秒内通过。覆盖正常解码、边界、stale 恢复、未知 ID、错误 DLC、原门槛 Burst、周期时序及负载下 NVM。负载下 UART 诊断与 UI flush 计数继续更新。最终镜像的直接视觉/触摸确认仍待完成。

Burst：原门槛为每 10 ms 五帧，持续约两秒，未降低阈值。PC 计划发送 1052 帧，原生驱动接收 1052 帧，drop=0。owner 异步发布边界处 runtime accepted/dispatched 增量为 1013；不能将差值误称为已证明丢包，也不能声称精确逐帧端到端一致。此负载窗口内 runtime overflow、原生 drop 和错误计数均未增长。

Periodic timing：约六秒、输入 500 帧/秒，使用 PCAN 原生时间戳。下表仅描述本次运行，不是车辆级时序保证。测试报告时序与长间隙，不虚构允许的 jitter 阈值。

| 帧 / 周期 | 样本数 | 最小间隔 | 最大间隔 | 平均间隔 | 最大绝对偏差 | 长间隙 |
|---|---|---|---|---|---|---|
| 0x3C0 / 50 ms | 121 | 48.969 ms | 51.017 ms | 49.993 ms | 1.031 ms | 0 |
| 0x2F0 / 100 ms | 61 | 98.962 ms | 101.009 ms | 99.986 ms | 1.038 ms | 0 |

Protocol 报告计划 missed=0、最大调度迟到 1 ms。线上时序还包括队列、驱动与仲裁延迟。

NVM coexistence：500 帧/秒负载下，亮度 revision 20 成为 DURABLE，dirty=0、存储错误为零；UI flush 从 350 增至 439。Runtime 继续处理 5969 帧，无新增原生 drop、队列 overflow 或 CAN 错误。测试恢复原设置。后续最终停机测试将亮度恢复为 75，durable revision 为 24。

OTA throughput：固件 i（ea048a4）在 500 kbit/s 总线上、约 500 帧/秒合成输入竞争下安装最终固件 j（368eed1）：1098752 字节 / 96.930 秒 = 11335.5 B/s（十进制 11.34 KB/s）。这是传输至 candidate-ready 的时间，不含完整重启耗时。候选校验、激活、重启及新 meter info 身份确认通过，NVM 设置恢复。证据：evidence/runtime-production/install-j。完整下载运行于发送前的固件 i；最终 j 另行通过 HIL 和下载中停机验证。

OTA mode test：下载 1024 字节后 Abort -> maintenance off -> NORMAL，speed 恢复新鲜 VALID；再次进入维护会话完成升级。完整传输内部第 5-90 秒，抓包含 1700 帧关键 0x3C0、零帧普通 0x2F0。OTA 抓包时间戳是 Host 到达时间，不能作为精确线上 jitter 证据。OTA queue_rejected/tx_errors 保持零，既有生命周期 TX 拒绝计数在该传输前后未增加。

Shutdown：最终 j，evidence/runtime-production/shutdown-j3。在下载 4096 字节且设置尚待持久化时，协作停止用时 2.768 秒：Update ABORTED、durable revision 23、CAN 关闭、UI unavailable、新设置被拒绝。owner 线程正常退出；重启恢复亮度 70，随后测试将亮度 75 持久化恢复。保留先前 shutdown-j/j2 脚本准备失败记录：期望状态名称错误，以及对已超时会话调用 Abort；这些不被计入固件 PASS。

## 资源测量

Memory：最终 ELF text 1078252 字节、data 12652 字节、BSS 211552 字节。这是链接静态统计，不含 LVGL/原生 heap 的总 RAM 峰值。语义 batch pool 42752 字节；包含两组栈/队列的 TX owner 结构合计 8072 字节。未增加 common 层运行期 heap。

Thread stack high-water：最终 j 在下载中停机前的 RT-Thread 整数百分比观测：Protocol 25%、App 26%、TX0 35%、TX1 26%、UI 17%、NVM 23%、Update 16%。这是已执行路径峰值，不是最坏情况保证。厂商软件 timer 的 512 字节栈达到 81%，需后续裕量审查；未修改 SDK 栈。

Queue high-water：最终 j 启动后连接负载显示语义批次 5/64、TX0 1、TX full=0。后续 HIL 生命周期 TX0 high=16/full=118 已存在于测量窗口之前，窗口内未增长；唯一 ACK 对端断开会产生预期驱动失败和有界入队拒绝。Batch full/rejected=0、runtime overflow=0。不能把整个启动周期描述成 TX 拒绝为零。多总线公平性已实现，尚未实板验收。

## 已知限制与剩余生产门禁

仅连接一个物理 PCAN 接口。匹配 Mixed Product 固件的双总线同时 HIL 仍未完成。Reference-Mixed/CANopen 测试属于 Host 证据。当前板端 smoke 构建仍选择参考 Demo；通用 External firmware composition 未由本次重构重新验收。

合并前需要运行新增 Linux quality job，本报告不宣称远端 CI PASS。还需完成最终视觉/触摸检查、长时间调度与故障压力、各 Flash/backend 阶段停机、设备相关 jitter 限值和 heap/stack 最坏情况测量。短时间 HIL 和源码 owner 门禁不能证明生产安全或无竞争。

原生 A/B candidate 激活与新镜像启动通过。确认机制仍为 native_auto；rollback、掉电恢复、签名和 anti-rollback 未重新验证。本结果可用于集成评审，尚不构成无限制生产发布验收。
