# 动态周期 TX

## 所有权与资源

App 将 Product 语义采样到 staging，再持原生优先级互斥锁进行有界深复制。Protocol 持同一短锁复制完整 publication，在编码前释放。Encoder 输入为 const 语义值，禁止访问 Core、设备、IPC、存储、UI 或堆。TX worker 仅消费复制报文并返回带身份结果，Protocol 独占 rolling state 提交，App 独占全局模式；局部 freshness 策略不执行全局迁移。

板级预算头文件声明可覆盖的周期槽及语义值容量（默认 8 和 16），启动时拒绝超预算 Product。可移植契约不限制产品表规模。三份语义数组、每周期项一个有界 mailbox，与普通/urgent 队列独立。锁只保护复制，不覆盖编码、驱动等待或日志。第一版 wire state 为 Product 定义的 uint32 token，复杂事务仍归 adapter/service。

## Publication 与期限

Generation 隔离模式/会话变化。Revision 仅在值/有效性/数量变化时递增，采样和发布时间变化不递增。同值新样本刷新 sample time，重新发布则保留。有效样本时间倒退/在未来时按单调半周期规则原子拒绝。Product 声明值和时间单位，revision 耗尽拒绝修改而非复用身份。

周期报文携带 generation、ticket、revision、取得/发布时间、计划 deadline 和 expiry。新 generation 复位 wire state，一整周期后再发送。保留既有 planned-deadline late-skip 调度器，无 catch-up burst、不做 nearest-deadline 优化。验收为 Protocol 取得新语义后的首个允许 deadline 使用该版本或更新版本。取得 snapshot 不以 revision 变化为条件，单纯 timestamp 刷新也会复制。

默认 SKIP_BUSY 在槽 pending/inflight 时跳过 deadline。REPLACE_PENDING 仅在新 deadline 替换未开始的报文，不能替换 inflight。Expiry 默认一周期，可设更短；TX 进入驱动前检查到期及代次。模式迁移无法取消硬件已在发送的帧。两种策略都不积累历史。每四个 urgent 给普通/周期一次机会，普通与周期公平竞争，各周期项轮转。

## Freshness 与 wire 提交

每项选择依赖区间、最大 sample age（零禁用时间超时）、HOLD、ENCODE_INVALID 或 SUPPRESS。即使禁用超时，无效样本仍无效。HOLD/ENCODE_INVALID 把 fresh=false 交给纯 Product encoder，Product 决定保持值、fallback 或标志，不新增自动全局 DEGRADED 迁移。

Admission commit 在 mailbox 接受后推进，即使后续取消也不撤销。Driver commit 仅由 Protocol 消费匹配身份的成功结果后推进。忙、失败、拒收、取消/过期和旧结果均有测试，不宣称远端确认推进策略。

SDK 既有 interrupt TX 路径等待完成。HAL 仅在 TX 中断且 TXB/TXC 状态置位时产生 TX_DONE，失败走 TX_FAIL。RT-Thread 仅在成功 completion 返回写入字节数。这是本机 controller/driver 完成，不是远端业务确认或 wire 时间戳，无需修改 SDK。PCAN 原生时间戳仍是独立 wire 测量。

## 合成 Demo 与证据

CAN0 的 0x3C0 / 50 ms 承载本机亮度，critical、driver commit；0x2F0 / 100 ms 承载 0.01 km/h 车速，ordinary、admission commit。均为 8 字节：小端值（0..1）、freshness（2）、counter（3）、revision 低 16 位（4..5）、byte 0 反码（6）、bytes 0..6 异或（7）。这是公开合成测试报文，不控制车辆。App 保留车速 sample time，每次 App 迭代采样本机亮度。

Shell 分别报告每项调度迟到、队列驻留、驱动耗时；首帧记录把取得时间/deadline/revision/counter 关联结果。诊断 revision/ticket 展示低 32 位，wire revision 为低 16 位，仅辅助关联，不用于运行时身份判断。物理时间来自 PCAN capture，之前约 1.04 ms 只是观测，不是需求。

Host 契约、原生 IPC、并发深复制测试与实板 HIL 分开。交付记录精确源码/构建/包 hash 及总线覆盖；Host 通过不能推出多总线、正式 MISRA、rollback 或断电恢复能力。
