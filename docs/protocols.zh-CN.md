# 协议与 Domain 契约

## Domain 与发布修订号

Core revision 比较完整 meter_value_t，包括时间戳、状态、来源和值；参数、故障、连接与设置变化也更新 Core revision。它与 Dynamic TX semantic revision 不同：后者仅对语义值/有效性变化递增，同值新样本可只刷新 sample time。每个 signal 的 stale_ms 为零表示不自动超时，不采用全局默认超时。来源仲裁接收 current/incoming 完整值。参见[动态 TX](dynamic-periodic-tx.zh-CN.md)。

## 协议服务边界

### 当前 API

Adapter 统一使用 on_frame/process/command/reset。meter_protocol_services_t 提供 Domain update、瞬时 Event、CAN TX，各回调具有独立上下文。Application 通过 meter_runtime_command 提交业务命令，由 Product 路由选择适配器。临时 owner API 与历史双回调已删除。

### 事务

Signal 是持续状态，Event 是瞬时事件，Command 是业务请求，Transaction 跟踪请求生命周期。CANopen SDO 事务属于独立调度器和上游客户端。Runtime 没有单例事务或传输超时。旧通用池在唯一生产用户迁移后已删除。

### 边界

产品工作流在传输回调之外消费读写结果。Core 不知道 PDO/SDO，UI 不知道 CAN，CANopenNode 不包含客户业务，Runtime 没有产品分支。协议调用串行执行，线程边界使用 RT-Thread 原生 IPC。

## CANopenNode 依赖与能力

### 依赖

CANopenNode 固定 v4.1、ac2140717c3c498d9b0351bce052bab630a74764，位于 third_party/CANopenNode（Apache-2.0，无本地补丁）。用途为模块级 SDO Client 协议引擎。

### 选择

产品在 product/sources.json 的 features 数组声明 canopennode-sdo。METER_ENABLE_CANOPENNODE 启用独立目标，Demo 默认关闭，需要 SDO 的产品拒绝显式 OFF。固定 PDO Binding 独立。能力描述支持选择功能，并非全局禁止其他产品使用 NMT/Heartbeat。

### 验证

Reference-Mixed 选择 PDO RX/TX 和 SDO Client，不使用 NMT/Heartbeat。二进制测试证明隔离，宿主测试调用真实上游模块。完整 CO_t 生命周期、EMCY/SYNC 和硬件验收不属于此次 SDO 接入。

## CANopenNode 模块级 SDO 接入

### 设计结论修正

完整 CO_t/CO_process 生命周期不适合当前 PDO/SDO 子集，但独立 CO_SDOclient_t 适用：它仅需静态 CAN 驱动桥和 0x1280 客户端参数记录，不依赖 CO_new、堆分配、NMT 或 Heartbeat。此前相反结论已删除。固定 PDO 保留平台静态 Binding 写入 Domain。CANopenNode v4.1（ac2140717c3c498d9b0351bce052bab630a74764）负责全部 SDO 编码、快速式/分段状态机、toggle 校验和协议超时，上游子模块未修改。

### 构建与所有权

生产仅编译上游 301/CO_SDOclient.c、301/CO_fifo.c、301/CO_ODinterface.c。开启 segmented 和 FIFO，关闭 block、本地传输和动态 OD。桥接全部静态分配。最小 OD 只含 0x1280 子索引 0 至 3，请求通过 CO_SDOclient_setup 选择远端。产品清单声明 canopennode-sdo，CMake 启用目标，显式冲突的 OFF 会报错。Demo/Reference-B 不链接客户端。CO_SDOserver_t 仅为宿主测试对端，不进入产品/固件。不编译 CANopen.c、NMT 或 Heartbeat 源码。

### 调度器与传输

每通道四个请求/结果槽、一个在途传输、FIFO 排队和 128 字节载荷上限。请求 ID 独立于 OD 索引/子索引，在占用槽中唯一。产品提交 READ/WRITE，结果在取走前保留：PENDING、SUCCESS、ABORTED、TIMEOUT。复位取消在途/排队工作，留下可读取的 ABORTED 结果。

CANopenNode 负责单次传输超时；调度器负责有界重试、间隔和默认关闭的 abort 重试。写入重试保留操作和原始载荷。旧通用事务池已删除，因为没有非 SDO 生产用户。Runtime 不维护第二套超时。

发送返回 false 时保留上游原始 TX 缓冲，随后 flush，bufferFull 防止覆盖。上游超时后明确取消尚未发出的过期请求，为上游 abort 释放缓冲。待发 abort 刷出后才能开始下一请求或重试。复位明确取消传输缓冲。

### 线程边界

桥、调度器和接收入口均由一个 Protocol 线程串行调用，ISR/其他线程必须通过 RT-Thread 原生 IPC 投递帧。空临界区宏依赖此所有权契约，不支持 ISR 直接并发调用。App/Core 保持独立单写者，UI 不访问 CANopenNode。生产 owner 见 runtime-production.zh-CN.md；Host 测试不能证明原生调度。

### 验证

真实上游客户端/服务端测试覆盖 1/2/4 字节快速式读写、标准 0x2B 两字节写、17/128 字节分段传输、abort code、超容量读、超时、读写重试、错误/迟到响应、复位/重连、TX busy、队列满。Mixed 测试覆盖有效 PDO 启动门控、失败不误报 READY、命令路由和 PDO freshness 恢复。

二进制门禁要求所选产品和 SDO 库含 CO_SDOclient 符号；拒绝产品中的 CO_new、CO_process、CO_NMT*、CO_HBconsumer*、CO_SDOserver*，以及 SDO 库中 malloc/calloc/realloc/free 的定义或引用。宿主可执行文件的 CRT 分配不属于模块无堆边界。Demo/Reference-B 可执行文件不得含 SDO 客户端符号。

### 限制

SDO 帧没有应用请求 ID。空闲/重试等待响应、错误长度和错误 COB-ID 被拒绝，错误对象响应交给上游。新传输开始后，相同对象的旧响应或相同 toggle 的旧分段无法可靠区分。产品须限制延迟、选择合适重试静默期，或在复用前复位。本地复位不能回滚远端已接受的写入。

FIFO/载荷上限为 128 字节，不支持 block，不产生 NMT/Heartbeat 流量。硬件 bus-off、电气行为和实板验收须独立测试。宿主对端不证明固件或实板验收。

## Reference-Mixed 服务迁移

### 组成

CAN0 保留合成 DBC/编解码器。CAN1 保留 RPDO 0x20C 静态 Domain Binding 和 TPDO 0x18C，产品 UI 不变。SDO 0x60C/0x58C 使用独立客户端/调度器。NMT/Heartbeat 关闭，PDO freshness 提供通信状态。

### 工作流

services/startup_parameter_sync.c 中的 StartupParameterSyncService 等待有效 PDO，读取 0x2000:01，确认成功后读取 0x2000:02，两项成功才进入 READY。值保存在产品服务状态中，只解释参数载荷，不解析 CAN 帧字节。终态失败进入 FAILED 并发事件；PDO 丢失取消工作并返回 WAIT_DATA，恢复后重新同步。

应用命令经过 meter_runtime_command、Product 路由、Adapter 和调度器。两字节写入使用上游 0x2B，非法命令被拒绝。请求 ID 为独立序号，复位清理产品工作流/通道状态。

### 测试

mixed-domain 保留 DBC 回放/stale 测试。mixed-canopen 使用上游宿主服务端验证 WAIT_DATA、A/B 顺序和值、两项参数写入、abort/重试耗尽不误报 READY、断连/复位、PDO 超时/恢复。canopennode-sdo 验证传输和分段，canopen-binary 验证符号隔离，架构/public-clean/公共头文件门禁保持启用。

### 实板边界

Reference-Mixed 的 Product/UI、PDO/SDO 和启动服务通过 Host 测试；单总线 Demo 实板结果不代表 Mixed 双总线验证。实际固件需选择相应 Product，并验证 CAN1、传输线程与硬件接线。
