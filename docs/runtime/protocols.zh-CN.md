# 协议与 Domain 契约

## Domain 与发布修订号

Core revision 比较完整 meter_value_t，包括时间戳、状态、来源和值；参数、故障、连接与设置变化也更新 Core revision。它与 Dynamic TX semantic revision 不同：后者仅对语义值/有效性变化递增，同值新样本可只刷新 sample time。每个 signal 的 stale_ms 为零表示不自动超时，不采用全局默认超时。来源仲裁接收 current/incoming 完整值。参见[动态 TX](dynamic-periodic-tx.zh-CN.md)。

## 协议服务边界

### 当前 API

Adapter 统一使用 on_frame/process/command/reset。meter_protocol_services_t 提供 Domain update、瞬时 Event、CAN TX，各回调具有独立上下文。Application 通过 meter_runtime_command 提交业务命令，由 Product 路由选择适配器。临时 owner API 与历史双回调已删除。

### 事务

Signal 是持续状态，Event 是瞬时事件，Command 是业务请求，Transaction 跟踪请求生命周期。Runtime 没有单例事务或传输超时。旧通用池在唯一生产用户迁移后已删除。

### 边界

产品工作流在传输回调之外消费读写结果。Core 不知道 PDO，UI 不知道 CAN，Runtime 没有产品分支。协议调用串行执行，线程边界使用 RT-Thread 原生 IPC。

## Demo 仪表发送 PDO

单一 Demo Product 新增两路公开合成标准 CAN0 数据帧，周期 100 ms、DLC 8、小端。仅发送定义位于 `products/demo/protocol/can/demo_tx.dbc`；接收 `demo.dbc`、其生成适配器和路由不变。这些示例不实现 CANopen NMT、SYNC、心跳、可配置 PDO 映射或对象字典，不代表客户协议或车辆控制指令。

| 帧 | 字节 / 位 | 含义 |
|---|---|---|
| 0x381 demo_tpdo_motion | 0–1 / 2–3 / 4–5 / 6 | 车速 0.01 km/h / 起升高度 0.001 m / 载荷 kg / SOC 百分比 |
| 0x381 demo_tpdo_motion | 字节 7 位 0 / 位 1–7 | 整组 fresh / 7 位序号 |
| 0x481 demo_tpdo_status | 字节 0 位 0–4 | 座椅占用 / 制动有效 / 空挡 / 充电 / 警告 |
| 0x481 demo_tpdo_status | 字节 1 位 0 / 字节 2 / 字节 3 | 整组 fresh / 8 位序号 / 布局版本 1 |
| 0x481 demo_tpdo_status | 字节 4–7 | Runtime 会话代数，不是固件版本或持久化计数 |

未使用位归零。运动组依赖车速、SOC、高度和载荷；状态组依赖五个布尔源值。App 保留源时间戳。任一源不可用、越界或年龄 >= 500 ms 时整组失效（Domain 过期策略可能更早失效）。失效组发送零测量值/状态位与 fresh=0；合法零值仍发送 fresh=1。接收者必须先检查 fresh。计数与布局/会话元数据在失效时仍有效。归一化布尔输入只接受精确 0 或 1。浮点数先检查范围，再缩放并向零截断，与既有车速编码一致。

两路新增帧均为普通流量，升级维护模式停发。序号模 128/256 回绕，只在本机驱动成功完成后推进，不代表远端确认；Runtime 代数变化时重新开始。计数或状态位不构成安全保证。既有 0x3C0、0x2F0 的载荷、依赖、周期和提交策略不变。所有帧共用既有发布 revision；新增 PDO 的语义变更也可能推进旧帧携带的 revision。

`demo-pdo` 检查源有效性、线协议黄金向量、合法零值、新鲜度边界、采样时间、驱动完成与静态预算。`demo-pdo-dbc` 使用 cantools 解码真实 C 输出。实物 `test_demo_instrument_pdo` 通过 PCAN 和发送 DBC 检查载荷、输入丢失/恢复和线序号；Host 通过不能证明板端发送。实际 Product 仍须独立确认映射和策略。
