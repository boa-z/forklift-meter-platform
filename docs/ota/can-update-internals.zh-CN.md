# CAN OTA 技术说明

> [English](can-update-internals.md)

构建更新包和刷入设备请先看[操作指南](can-update.zh-CN.md)。本页保留实现约束和历史排查记录，不作为操作步骤。

## 设计结论

CAN OTA 是可选 Framework 能力。目标路径为 CAN → ISO-TP/UDS → 有界任务副本 → 单一 Update worker → 现有 ArtInChip OTA 安装器 → 非活动 A/B 候选 → NVM durable 门禁 → 激活 → 重启 → 新固件观测。

Product 决定准入与维护模式，Core/UI 不解析 CAN、不调用 Flash。不在 RAM 缓存完整固件，不自制 CAN 文件分片协议。

可选 Update worker 复用生产架构统一的 Protocol/CAN TX owner 与未经修改的 ArtInChip 原生安装器。传输完成、候选验证、激活和新固件观察保持区分。

SDK 在应用健康检查前清除升级标志，native_auto 不等于应用控制的确认。SHA256 是完整性检查而非签名；rollback 和下载中掉电仍未验证。

## SDK 审查

审查基线：SDK 9b78386dcaa54326487c8b208b2b7104d4b56c9b，受跟踪目录树与 7608a666 相同。不修改 SDK 源码、配置或父仓库 gitlink。

| 范围 | 观察到的实现 | 影响 |
|---|---|---|
| packages/artinchip/ota/ota.h | 提供 ota_init、ota_deinit、ota_shard_download_fun | 没有候选策略或完整包完成查询 API |
| packages/artinchip/ota/ota.c | 包元数据选择分区，收到文件头即可触发擦除 | 转发字节前必须有应用层受限包策略，不能仅依赖 Host 检查 |
| packages/artinchip/ota/ota.c | 检查逐文件加和与读回加和 | 不能代替完整包 SHA256 和严格尾标记/长度验证 |
| packages/artinchip/env/absystem_os.c | aic_upgrade_end、aic_ota_status_update 不传递 fw_env_flush 返回值 | 返回成功不足以证明激活/确认已持久化 |
| packages/artinchip/env/absystem_os.c | 挂载路径在应用健康/NVM 验证之前调用 aic_ota_status_update | 该基线不能宣称应用控制的试运行确认 |
| packages/artinchip/uds | 现有示例下载处理器执行同步文件 I/O | 不直接照搬到生产 Flash worker 契约 |
| 当前组合 | SDK OTA downloader 关闭；可选应用组合链接原样 ota.c/burn.c | 仅 METER_CAN_UPDATE=1 提供新端点 |

默认拒绝后端保留为 Host 边界测试。METER_CAN_UPDATE=1 选择 meter_update_backend_aic.c：调用原生 ENV API 前独立验证物理冗余 ENV CRC，拒绝待激活或不一致槽位及重叠分区，仅接受经审计的 2048 字节页、每块 64 页、共 32 块的非活动 OS 分区。

候选区必须没有坏块。Flash 擦写、读取与 A/B 选择继续由厂商实现。

未启用 NFTL 时，应用侧为缺失的可选初始化符号提供明确拒绝实现；不开放数据分区升级。

完整元数据与 OS 头通过校验后才交给原生安装器擦除候选区，随后仅转交完整的 4096 字节 OS 块。完整包、尾标、长度、SHA256 和已安装 OS 的 SHA256 回读验证均通过后才允许激活。

激活前先等待 NVM durable barrier；原生 API 忽略 flush 失败，因此激活后关闭并重新读取 ENV 验证。失败不自动重启。

此受限集成不代表应用健康确认或 rollback 已实现。

## 依赖与资源边界

固件协议复用 iso14229 发布版 0.11.0（提交 8a7eb23d6ab9d52f66ce36b07ba456613709c78e）及其内置 isotp-c，均为 MIT 授权，固定子模块保留上游声明，不修改上游业务逻辑。Host 依赖固定于 tools/ota/requirements.txt：python-can 4.6.1（LGPL-3.0）、can-isotp 2.0.7（MIT）、udsoncan 1.25.1（MIT）。

这些是 Host 依赖，不进入固件。

UDS 收发缓冲各 1024 字节，每个复制的升级块最多 512 字节。所有标识符的线协议只在 protocols/uds/meter_uds_did.h 定义一次，字段与语义见 [UDS DID 参考](#uds-did-参考)。

Host 预检每次最多读取 64 KiB，传输块最多 512 字节。同一打开文件在发送过程中再次计算 hash，内容变化时在验证之前中止。

CLI 遵守设备块长与接收端 STmin/流控。独占维护配置请求零 STmin、每八帧流控，移除原来的 Host 20000 bit/s 数据限额，但不会覆盖较慢接收端的 STmin。

固件通过上游配置采用一毫秒响应调度延迟；挂起操作保持原有有界超时。维护模式以外的正常/Burst HIL 门槛保持不变。

## UDS DID 参考

protocols/uds/meter_uds_did.h 是本契约的唯一定义处，下表必须与之一致。0xF180（bootSoftwareIdentification）已删除，不得再实现。0xFD00-0xFEFF 是 ISO 14229-1 保留给系统供应商专用的区间，升级相关标识符落在此处；F186/F192/F195 为标准供应商标识符。

| DID | 名称 | 长度 | 服务 | 限制 |
|---|---|---|---|---|
| 0xFD10 | 升级 Manifest | 148 | 0x22 读、0x2E 写 | 0x2E 仅编程会话 |
| 0xFD11 | 升级状态 | 12 | 0x22 读 | 无 |
| 0xFD12 | 升级端点 | 164 | 0x22 读 | 无 |
| 0xF186 | 当前诊断会话 | 1 | 0x22 读 | 无 |
| 0xF192 | 硬件号 | 32 | 0x22 读 | 无 |
| 0xF195 | 软件版本 | 48 | 0x22 读 | 无 |

一次 0x22 可携带多个标识符，响应按请求顺序拼接。每个标识符都是定长，Host 期望其他长度属于协议错误，而不是"更短或更长的值"。CAN 上不再承载 JSON：UART 的 `meter_update info` 仍输出 JSON，但它与这些标识符取自同一份设备快照。

### FD10 升级 Manifest（148 字节）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 32 | Product 标识 |
| 32 | 32 | Hardware 标识 |
| 64 | 48 | 包版本 |
| 112 | 4 | 包长度，大端 uint32，非零 |
| 116 | 32 | 完整包的 SHA256 |

文本字段为 ASCII、NUL 终止并补零到字段宽度，允许字符为 A-Z a-z 0-9 . _ + -。字段宽度内没有 NUL、出现非 ASCII 字节、字段为空或包长度为零都拒绝写入。字符串永不截断：身份字符串超长在启动阶段直接失败。没有任何权威 Manifest 时读 FD10 返回 148 个零字节。

### FD11 升级状态（12 字节）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 1 | 升级状态 |
| 1 | 1 | 最近错误 |
| 2 | 1 | 标志位 |
| 3 | 1 | 保留，固定为 0 |
| 4 | 4 | 已接收字节，大端 |
| 8 | 4 | 总字节，大端 |

状态：0 空闲、1 下载中、2 传输完成、3 校验中、4 候选就绪、5 等待持久化、6 激活中、7 已激活、8 已确认、9 已中止、10 失败。错误：0 无、1 状态、2 兼容性、3 拒绝、4 长度、5 顺序、6 后端、7 哈希、8 超时、9 NVM、10 会话、11 格式、12 不支持。两张编号表都用指定初始化器，重排 C 枚举不会改变线上字节。

标志位：bit0 后端可用、bit1 维护中、bit2 独占维护、bit3 准入通过、bit4 Manifest 存在、bit5 候选已验证。bit6-7 保留且恒为 0。

进度按状态定义：空闲、已中止、已确认返回 0/0；下载中返回实时值；传输完成、校验中、候选就绪、等待持久化、激活中、已激活返回 总长/总长；失败保留失败瞬间的值用于诊断。总长绝不由"存在 Manifest"推断。

### FD12 升级端点（164 字节）

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 32 | Product 标识 |
| 32 | 32 | Hardware 标识 |
| 64 | 48 | 当前运行版本 |
| 112 | 16 | 包内 OS 成员名 |
| 128 | 4 | 非活跃槽容量，大端 |
| 132 | 32 | 准入拒绝原因 |

文本编码与 FD10 相同。OS 成员名与拒绝原因按固定字段定宽，不存在更长的诊断字符串，因此原生后端上报 env_or_slot_invalid 或 native_auto_unverified，拒绝型后端上报 sdk_validation_confirm_unavail。原因为空表示未被拒绝。

### F186、F192、F195

F186 返回一字节会话类型。F192 返回 32 字节硬件身份，F195 返回 48 字节 OTA 固件版本，取自 METER_UPDATE_FIRMWARE_VERSION。显示版本 METER_FIRMWARE_DISPLAY_VERSION 只是界面文本，永不通过 F195 暴露。

### 服务权限与 NRC

0x2E FD10 只校验标识符、编程会话、148 字节长度与字段语法：标识符不符或不可写为 0x31，非编程会话为 0x7F，长度错误为 0x13，文本非法或包长度为零为 0x31，已存在候选、传输进行中或请求未完成为 0x22。它不检查 Product/Hardware 匹配、容量与准入，也不触碰 Flash。

0x34 负责准入与传输本身：没有 staged Manifest 为 0x22；地址非零、数据格式标识符非零或长度与 Manifest 不符为 0x31；Product/Hardware 不匹配、容量不足与准入失败通过 worker 错误映射返回。两个服务因此不共用一张 NRC 表。

Manifest 生命周期：

- 0x2E FD10 保存 staged Manifest，在任何下载开始前即可通过 FD10 回读。
- 0x34 把权威转移给 worker。异步 Begin 最终完成时（无论成功与否）丢弃 staged 副本，因此之后的失败或中止状态不会暴露上一请求的 Manifest；0x78 挂起期间不会提前丢弃。
- worker Manifest 仅在下载中、传输完成、校验中、候选就绪、等待持久化、激活中、已激活时有效。失败与中止读作 148 个零字节。
- 失败之后重新执行 0x2E FD10 会成功，即使尚未开始新下载，FD10 也可再次回读。
- 会话超时与 0x10 01 丢弃 staged Manifest 与未完成的传输，但保留已验证候选，其所有权仍在 worker。

正常刷写序列：

```text
0x10 02                     进入编程会话
0x22 FD12 FD11              可选预检：端点与状态
0x2E FD10 <148 字节>        提交 Manifest
0x34                        地址零、声明长度
0x36 <1..512 字节> × N      传输，标准模 256 序号
0x37                        设备验证
0x31 01 F001                激活
0x11 01                     复位，随后通过 F195 观察新版本
```

0x31 01 F002 中止并丢弃候选；0x31 01 F003 为设备确认保留。只有激活之后才接受 ECUReset，CLI 不将发送重启等同新固件确认。

## OS-only 包策略

update/meter_package.c 中的共享 C 校验器接受 aic-os-only-crc-cpio-v1，Host 检查器与板端 worker 使用同一实现。上下文小于 1 KiB，无堆分配，支持任意输入边界。

适配层另外维护包与镜像的流式 SHA256，不缓存完整固件。

仅接受 CRC CPIO（070702），成员顺序必须为 ota_info.bin、Product 指定的 OS .itb 文件、TRAILER!!!。元数据必须为 512 字节，声明准确包长和目标版本，仅允许 ota_info.bin:file 与 OS 文件:os 映射。

检查文件名、普通文件类型、单链接、成员校验和、零值对齐填充及最终 512 字节归档对齐。拒绝额外分区、重复成员、路径穿越、其他目标映射和截断包。

OS 大小必须满足实际非活动分区容量按厂商 4096 字节写入粒度向下对齐后的限制。容量由集成者/设备提供，不从归档中信任。

原生元数据限制版本为 31 个 ASCII 字符，通用 UDS manifest 的版本字段仍为 48 字节。

包元数据的四字节前缀由 CPIO 加和及外层 SHA256 覆盖，它与实际启动 ENV 不同：选择槽位前独立验证物理 ENV CRC。擦除前检查 FIT 头及其声明大小，原生回读后验证完整 OS SHA256。

这些检查不认证固件内部 Product，也不能证明可启动；FIT 加载继续由 Bootloader 负责。

SDK mkcpio.py 在成包后修改元数据而没有重建 CPIO 校验和，还会编辑输入配置。Framework pack 在临时目录调用未修改的 mkenvimage 与 GNU cpio 两次，校验后发布 ota.cpio、ota.manifest.json 和 package-report.json。

--pad-os-to 4096 只对临时 OS 副本补 0xFF，填充计入 CPIO 校验和与包 SHA256；板端适配必须使用该选项。manifest 的 Product/Hardware 来自操作者，不构成固件身份认证。

Linux CI 安装 cpio 和 u-boot-tools（mkenvimage）；Windows 可显式传入 SDK 工具的可执行文件路径。测试使用真实原生工具和公开合成载荷，合成 .itb 数据不是可启动镜像。

## 验证与剩余工作

连接前通过 UART 读取 `meter can 0`，将其实际速率显式传入 `--bitrate`。CLI 默认值不是 Product 默认值，速率不一致时即使接线正确也可能 bus-off。

发现 PCAN 通道不代表已确认它连接的板端 CAN 插座。

激活屏障遵循 Product 存储契约：未配置或显式禁用存储时没有待持久化的设置版本；启用存储时仍要求非零目标 revision 和成功的 durable 检查。已启用但不可用或失败的存储不能走无存储路径。

Host 回归覆盖无存储、禁用存储、无目标版本、持久化未完成及 durable 完成。

新增测试覆盖切块边界、CPIO/元数据异常、容量对齐、原生成包及失败不发布包。Host 测试还覆盖 manifest 边界、损坏、修改前身份/维护/能力拒绝、块序号回绕、文件变化、超时/Abort 和激活目标检查。

python-can 虚拟总线测试通过 can-isotp、udsoncan 交换真实分段 ISO-TP 响应并解析 CAN 证据。这是 Host 传输测试，不是 PCAN 实物或 OTA 安装。

C 测试覆盖状态转换、worker 完成、异常 UDS 请求及不链接 SDK 的实际拒绝后端。同时固定 DID 契约：标识符长度、各服务的 NRC、逐状态的进度语义、Begin 与会话超时及失败之间的 Manifest 生命周期，以及身份字符串超长时拒绝启动。Linux CI 新增可选升级配置，本机测试不代表远端 CI 已运行。

真实原生解包器/摘要 Host 回归覆盖任意切块、无效 ENV、不安全分区几何、异常元数据/FIT/尾标、坏块、擦写读失败、回读损坏、hash 不匹配、Abort 和 ENV 落盘丢失；这些测试中的 Flash/ENV 是替身。Host 故障测试不代表实际掉电或回滚验证。

## 板端测试构建

通过 SDK OneStep 构建，设置 `METER_CAN_UPDATE=1` 与 `METER_UPDATE_VERSION`，见[构建指南](../build/build.zh-CN.md)。构建成功后升级包写入 `output/<project>/ota/<version>/`。OTA 只通过应用构建设置启用。

厂商 OTA 对象文件放在 `output/<project>/meter/sdk-ota`，不修改 SDK 源码、配置或 gitlink。同一提交分别构建 ota-board-a 基线与 ota-board-b 候选，首次只刷基线 image，候选 OS 生成 CAN 升级包；重启后 version 字段应变化。

线程 owner、栈和 CAN 队列预算统一维护在[生产运行时](../runtime/runtime-production.zh-CN.md)。Update 保留一个 job 和一个 completion 信用，取消时须排空旧 job/result 后才能接受新任务。

只有 CAN TX worker 在 driver write 阻塞，App 拥有 Domain，UI 拥有 LVGL。原生 backend 有 1536-byte prefix、4096-byte 写入/回读缓冲，厂商 OTA 使用两个 8192-byte 缓冲和一个 4096-byte 回读缓冲；这些是固定容量，不是最坏栈测量。

合成 Demo 要求本地维护模式与 NVM ready，不具备车辆安全联锁。维护期间冻结设置写入，激活前最多等待五秒使目标 revision durable。

超时取消未完成传输，已验证候选可留待下一次 CLI 激活；Abort 删除候选但不能撤销激活。Product Mode 策略保留 critical 周期 TX，抑制普通 telemetry/TX，并用 generation 作废旧工作。

双语 update overlay 代替实时读数，百分比仅表示接收字节。诊断、ISO-TP/UDS、Update 和 NVM barrier 持续运行；只有 App 控制维护进入/退出。

专门测速可停止外部周期发送，负载共存及普通 HIL 不降低原门槛。

刷入后先验证 UI、触摸、meter info、NVM READY 及 backend_supported=true，不先打开维护模式。之后启用维护模式进行受控测试。

保留含原生槽位选择的 UART 启动日志、PCAN ASC、包 hash、升级前后版本和诊断。错误 Product、损坏包、传输中断、主动 Abort、维护进入/退出、UART 响应以及升级后普通 CAN 恢复分别留证。

人工断电属于独立测试，此次构建与 Host PASS 均不代表其结果。

## 实板传输观察

首次板测发现两个集成问题：同步 UDS 客户端等待完整 ISO-TP 响应，而协商的 150 ms P2 预算短于 5 ms STmin 下的分段 JSON 诊断响应；RT-Thread 控制台也会截断单次过长的格式化输出。Host 连接改为显式有界的两秒完整响应预算，保留 120 秒 response-pending 上限。

这是客户端重组预算，不代表已证明 ECU 满足 P2。串口诊断按有界片段输出。

回归先协商会话，再接收真实 ISO-TP 长响应；适配测试模拟 128 字节控制台缓冲。

CAN 证据保留 backend 原生 RX 时间戳，Host TX 观察可能使用不同时间原点。按日志顺序窗口与帧身份关联，在同一原生时钟内计算 RX 间隔，不将原生 uptime 与 Host epoch 直接相减。

周期背景流量复用唯一 python-can owner，在通道关闭前停止。
