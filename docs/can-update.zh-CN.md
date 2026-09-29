# CAN 固件升级：应用集成与板端验证

## 设计结论

CAN OTA 是可选 Framework 能力。目标路径为 CAN → ISO-TP/UDS → 有界任务副本 → 单一 Update worker → 现有 ArtInChip OTA 安装器 → 非活动 A/B 候选 → NVM durable 门禁 → 激活 → 重启 → 新固件观测。Product 决定准入与维护模式，Core/UI 不解析 CAN、不调用 Flash。不在 RAM 缓存完整固件，不自制 CAN 文件分片协议。

可选 Update worker 复用生产架构统一的 Protocol/CAN TX owner 与未经修改的 ArtInChip 原生安装器。传输完成、候选验证、激活和新固件观察保持区分。SDK 在应用健康检查前清除升级标志，native_auto 不等于应用控制的确认。SHA256 是完整性检查而非签名；rollback 和下载中掉电仍未验证。

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

默认拒绝后端保留为 Host 边界测试。METER_CAN_UPDATE=1 选择 meter_update_backend_aic.c：调用原生 ENV API 前独立验证物理冗余 ENV CRC，拒绝待激活或不一致槽位及重叠分区，仅接受经审计的 2048 字节页、每块 64 页、共 32 块的非活动 OS 分区。候选区必须没有坏块。Flash 擦写、读取与 A/B 选择继续由厂商实现。未启用 NFTL 时，应用侧为缺失的可选初始化符号提供明确拒绝实现；不开放数据分区升级。

完整元数据与 OS 头通过校验后才交给原生安装器擦除候选区，随后仅转交完整的 4096 字节 OS 块。完整包、尾标、长度、SHA256 和已安装 OS 的 SHA256 回读验证均通过后才允许激活。激活前先等待 NVM durable barrier；原生 API 忽略 flush 失败，因此激活后关闭并重新读取 ENV 验证。失败不自动重启。此受限集成不代表应用健康确认或 rollback 已实现。

## 依赖与资源边界

固件协议复用 iso14229 提交 2e36afcd7f0cd02b0c70446c1a265a7b1999d478 及其内置 isotp-c，均为 MIT 授权，固定子模块保留上游声明，不修改上游业务逻辑。Host 依赖固定于 tools/ota/requirements.txt：python-can 4.6.1（LGPL-3.0）、can-isotp 2.0.7（MIT）、udsoncan 1.25.1（MIT）。这些是 Host 依赖，不进入固件。

UDS 收发缓冲各 1024 字节，每个复制的升级块最多 512 字节。148 字节 manifest 为 Product[32]、Hardware[32]、Version[48]、大端 uint32 包长度、SHA256[32]。DID 0xF180 写 manifest、读 JSON 诊断。RequestDownload 使用地址零和准确包长，TransferData 使用标准模 256 序号，TransferExit 请求设备验证。StartRoutine 0xF001 激活、0xF002 中止、0xF003 为设备确认保留。设备激活后才允许 ECUReset，CLI 不将发送重启等同新固件确认。

Host 预检每次最多读取 64 KiB，传输块最多 512 字节。同一打开文件在发送过程中再次计算 hash，内容变化时在验证之前中止。CLI 遵守设备块长与接收端 STmin/流控。独占维护配置请求零 STmin、每八帧流控，移除原来的 Host 20000 bit/s 数据限额，但不会覆盖较慢接收端的 STmin。固件通过上游配置采用一毫秒响应调度延迟；挂起操作保持原有有界超时。维护模式以外的正常/Burst HIL 门槛保持不变。

## OS-only 包策略

update/meter_package.c 中的共享 C 校验器接受 aic-os-only-crc-cpio-v1，Host 检查器与板端 worker 使用同一实现。上下文小于 1 KiB，无堆分配，支持任意输入边界。适配层另外维护包与镜像的流式 SHA256，不缓存完整固件。

仅接受 CRC CPIO（070702），成员顺序必须为 ota_info.bin、Product 指定的 OS .itb 文件、TRAILER!!!。元数据必须为 512 字节，声明准确包长和目标版本，仅允许 ota_info.bin:file 与 OS 文件:os 映射。检查文件名、普通文件类型、单链接、成员校验和、零值对齐填充及最终 512 字节归档对齐。拒绝额外分区、重复成员、路径穿越、其他目标映射和截断包。OS 大小必须满足实际非活动分区容量按厂商 4096 字节写入粒度向下对齐后的限制。容量由集成者/设备提供，不从归档中信任。原生元数据限制版本为 31 个 ASCII 字符，通用 UDS manifest 的版本字段仍为 48 字节。

包元数据的四字节前缀由 CPIO 加和及外层 SHA256 覆盖，它与实际启动 ENV 不同：选择槽位前独立验证物理 ENV CRC。擦除前检查 FIT 头及其声明大小，原生回读后验证完整 OS SHA256。这些检查不认证固件内部 Product，也不能证明可启动；FIT 加载继续由 Bootloader 负责。

SDK mkcpio.py 在成包后修改元数据而没有重建 CPIO 校验和，还会编辑输入配置。Framework pack 在临时目录调用未修改的 mkenvimage 与 GNU cpio 两次，校验后发布 ota.cpio、ota.manifest.json 和 package-report.json。--pad-os-to 4096 只对临时 OS 副本补 0xFF，填充计入 CPIO 校验和与包 SHA256；板端适配必须使用该选项。manifest 的 Product/Hardware 来自操作者，不构成固件身份认证。

Linux CI 安装 cpio 和 u-boot-tools（mkenvimage）；Windows 可显式传入 SDK 工具的可执行文件路径。测试使用真实原生工具和公开合成载荷，合成 .itb 数据不是可启动镜像。

## CLI

从 Framework 根目录运行。将 CAPACITY_BYTES 替换为实际非活动分区容量（十进制字节）；OS 文件名也必须匹配 Product 策略。pack 可用 --cpio 和 --mkenvimage 指定原生工具；METER_OTA_INSPECTOR 可指定已构建检查器的路径。manifest JSON 必须包含 product、hardware、version、size、sha256 小写字段。它是打包流程提供的受信输入，不是内嵌固件身份的认证证明。预检核对文件与 manifest；包结构、安装后镜像与兼容性仍必须由设备验证。任意字节即使 hash 匹配也不等于有效 OTA 包。

~~~text
python -m pip install -r tools/ota/requirements.txt
cmake -S . -B build-package -DMETER_BUILD_UI=OFF
cmake --build build-package --target meter-ota-inspect
python -m tools.ota pack firmware.itb ota-output --product reference-demo --hardware reference-board --version v2 --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota preflight ota.cpio --manifest ota.manifest.json --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota --evidence evidence/ota-probe probe
python -m tools.ota --evidence evidence/ota-download download ota.cpio --manifest ota.manifest.json --os-file d13x_os.itb --candidate-capacity CAPACITY_BYTES
python -m tools.ota --evidence evidence/ota-activate activate --version v2 --reboot
python -m tools.ota --evidence evidence/ota-abort abort
~~~

PCAN 默认 PCAN_USBBUS1、500000 bit/s，请求 ID 0x7E0、响应 ID 0x7E8。证据目录必须新建，联网操作保存及时刷新的 JSONL 进度/错误与收发 CAN ASC 记录，不自动操作 PCAN-View。Probe 只读。下载要求 Product/Hardware 匹配、后端明确支持、设备 OS 文件名/候选容量兼容、维护准入、设备为 idle/failed/aborted；缺少能力声明即拒绝。下载不自动激活。激活必须明确匹配目标版本，重启另设开关。传输/会话失败尽可能请求 Abort；Abort 也失败时保留原始错误。断开的设备可能收不到 Abort，设备侧仍须执行自己的超时。

## 验证与剩余工作

连接前通过 UART 读取 `meter can 0`，将其实际速率显式传入 `--bitrate`。CLI 默认值不是 Product 默认值，速率不一致时即使接线正确也可能 bus-off。发现 PCAN 通道不代表已确认它连接的板端 CAN 插座。

激活屏障遵循 Product 存储契约：未配置或显式禁用存储时没有待持久化的设置版本；启用存储时仍要求非零目标 revision 和成功的 durable 检查。已启用但不可用或失败的存储不能走无存储路径。Host 回归覆盖无存储、禁用存储、无目标版本、持久化未完成及 durable 完成。

新增测试覆盖切块边界、CPIO/元数据异常、容量对齐、原生成包及失败不发布包。Host 测试还覆盖 manifest 边界、损坏、修改前身份/维护/能力拒绝、块序号回绕、文件变化、超时/Abort 和激活目标检查。python-can 虚拟总线测试通过 can-isotp、udsoncan 交换真实分段 ISO-TP 响应并解析 CAN 证据。这是 Host 传输测试，不是 PCAN 实物或 OTA 安装。C 测试覆盖状态转换、worker 完成、异常 UDS 请求及不链接 SDK 的实际拒绝后端。Linux CI 新增可选升级配置，本机测试不代表远端 CI 已运行。

真实原生解包器/摘要 Host 回归覆盖任意切块、无效 ENV、不安全分区几何、异常元数据/FIT/尾标、坏块、擦写读失败、回读损坏、hash 不匹配、Abort 和 ENV 落盘丢失；这些测试中的 Flash/ENV 是替身。绑定源码的实板吞吐、维护、NVM 和重启结果见[验证记录](validation.zh-CN.md)，Host 故障测试不代表实际掉电或回滚验证。

## 板端测试构建

使用 tools/ota/build_board.py，传入 --sdk-root、--version 和全新 --output 目录；必要时通过 --python 指定 SDK Python。它仅用应用构建参数启用 OTA，归档 image/ELF/map/OS/ENV 与来源身份，记录生效配置，并在 finally 原样恢复 .config 字节。厂商对象文件放在应用忽略目录 build-firmware/sdk-ota，不修改 SDK 源码、配置或 gitlink。同一提交分别构建 ota-board-a 基线与 ota-board-b 候选，首次只刷基线 image，候选 OS 生成 CAN 升级包；重启后 firmware_version 应变化。

线程 owner、栈和 CAN 队列预算统一维护在[生产运行时](runtime-production.zh-CN.md)。Update 保留一个 job 和一个 completion 信用，取消时须排空旧 job/result 后才能接受新任务。只有 CAN TX worker 在 driver write 阻塞，App 拥有 Domain，UI 拥有 LVGL。原生 backend 有 1536-byte prefix、4096-byte 写入/回读缓冲，厂商 OTA 使用两个 8192-byte 缓冲和一个 4096-byte 回读缓冲；这些是固定容量，不是最坏栈测量。

合成 Demo 要求本地维护模式与 NVM ready，不具备车辆安全联锁。维护期间冻结设置写入，激活前最多等待五秒使目标 revision durable。超时取消未完成传输，已验证候选可留待下一次 CLI 激活；Abort 删除候选但不能撤销激活。Product Mode 策略保留 critical 周期 TX，抑制普通 telemetry/TX，并用 generation 作废旧工作。双语 update overlay 代替实时读数，百分比仅表示接收字节。诊断、ISO-TP/UDS、Update 和 NVM barrier 持续运行；只有 App 控制维护进入/退出。专门测速可停止外部周期发送，负载共存及普通 HIL 不降低原门槛。

~~~text
METER_CAN_UPDATE=1
METER_UPDATE_VERSION=ota-board-a
meter info
meter storage
meter_update info
meter_update maintenance on
python -m tools.ota --evidence evidence/ota-probe probe
python -m tools.ota pack candidate.itb ota-output --product reference-demo --hardware reference-board --version ota-board-b --os-file d13x_os.itb --candidate-capacity 4194304 --pad-os-to 4096
python -m tools.ota --evidence evidence/ota-download download ota-output/ota.cpio --manifest ota-output/ota.manifest.json --os-file d13x_os.itb --candidate-capacity 4194304
python -m tools.ota --evidence evidence/ota-activate activate --version ota-board-b --reboot
~~~

刷入后先验证 UI、触摸、meter info、NVM READY 及 backend_supported=true，不先打开维护模式。之后启用维护模式进行受控测试。保留含原生槽位选择的 UART 启动日志、PCAN ASC、包 hash、升级前后版本和诊断。错误 Product、损坏包、传输中断、主动 Abort、维护进入/退出、UART 响应以及升级后普通 CAN 恢复分别留证。人工断电属于独立测试，此次构建与 Host PASS 均不代表其结果。

## 实板传输观察

首次板测发现两个集成问题：同步 UDS 客户端等待完整 ISO-TP 响应，而协商的 150 ms P2 预算短于 5 ms STmin 下的分段 JSON 诊断响应；RT-Thread 控制台也会截断单次过长的格式化输出。Host 连接改为显式有界的两秒完整响应预算，保留 120 秒 response-pending 上限。这是客户端重组预算，不代表已证明 ECU 满足 P2。串口诊断按有界片段输出。回归先协商会话，再接收真实 ISO-TP 长响应；适配测试模拟 128 字节控制台缓冲。

CAN 证据保留 backend 原生 RX 时间戳，Host TX 观察可能使用不同时间原点。按日志顺序窗口与帧身份关联，在同一原生时钟内计算 RX 间隔，不将原生 uptime 与 Host epoch 直接相减。周期背景流量复用唯一 python-can owner，在通道关闭前停止。
