# NVM 持久化

## 所有权与策略

App 独占 RAM 设置与 NVM 版本服务。RT-Thread worker 独占 EEPROM/文件 I/O 与槽缓存。原生单项消息队列传递请求和结果，同一时刻只有一个不可变在途副本；另一个 pending 缓冲保存较新设置。SDL Host 在独立线程中使用相同记录、服务与后端代码。worker 不读取活动 Core，UI 动作回调不做存储 I/O。

Reference-Demo 明确声明 namespace 0x444D、record type 1、schema 2、500 ms 防抖和 3000 ms 最大未保存窗口。其合成本机参数与语言、单位、亮度组成完整替代记录。External Product 必须选择自己的 namespace/schema 和本机权威策略，不能照搬后把远端 ECU 参数变成本机权威。存储版本独立于遥测/Domain revision，内容不变不增版。

## 编码与提交

FMP2 使用小端。MSP2 payload 按稳定参数 ID 编码为 32 位（限制在 Domain uint16 范围），数值为 IEEE binary32，不依赖目录顺序。缺项、重复、未知身份或越界值导致整体拒绝。内层 FNV 校验仅检测缓冲损坏，不是认证。外层 CRC 复用固定 CANopenNode 的 CRC-16/XMODEM（多项式 0x1021，初值 0，标准 check 0x31C3）。header 与 payload 连续计算，不再异或两段独立 CRC。test_meter_record.c 包含黄金字节与逐字节损坏验证。

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 4 | FMP2 魔数 |
| 4 | 2 | Version 2 和零保留字节 |
| 6 | 2 | Record type |
| 8 | 2 | Product namespace |
| 10 | 2 | Schema |
| 12 | 2 | Flags，当前为零 |
| 14 | 8 | 单调提交序号 |
| 22 | 4 | Payload 长度 |
| 26 | 2 | 头 0–25 字节接 payload 的 CRC |
| 28 | 4 | 保留，必须为零 |
| 32 | 可变 | MSP2 payload |
| 尾部 | 16 | 序号、长度、CRC 与 CRC 反码 |

每槽另有独立提交页，保存绑定的 16 字节 seal 副本。提交顺序为使 seal 失效并同步读回，写完整 body 并同步读回，最后写 seal 并同步读回，成功后才能切换活动槽。不写最后有效槽。扫描区分 EMPTY（全 0xFF）、VALID、CORRUPT、INCOMPATIBLE、I/O 错误和同代次内容冲突。一坏一好降级恢复；未知 schema 禁止自动覆盖。不迁移旧格式、不隐式格式化、不静默切换介质、不允许 sequence 回绕。

## 后端

| 后端 | 接入 | 保证与边界 |
|---|---|---|
| reference-board EEPROM | BL24C512A 系列，i2c0，0x50，64 KiB，128 字节页 | 已授权的新分配区 0x0000–0x03FF；A 位于 0x0000，B 位于 0x0200；seal 页为 0x0180、0x0380。初始化、保存、软重启恢复已验证；物理掉电未验证。 |
| Host 文件 | Prefix.0 和 Prefix.1 | 检查完整写、fsync/_commit、close 与读回；不宣称文件系统元数据掉电原子性。 |
| NOR/LittleFS 文件 | SDK 已挂载文件系统 | 使用同一可选文件适配，不实现裸 NOR 驱动或格式化；设备 sync/XIP 与实物恢复待 Board 验证。 |
| NAND/FATFS 文件 | SDK NFTL/ECC 与已挂载文件系统 | 弱文件系统保证，双文件不能证明元数据独立冗余；要求已验证 power-safe 能力时拒绝配置。 |

EEPROM transport 仅在应用层复用现有 RT-Thread I2C API，不修改 SDK、HAL、板级配置或父仓库 gitlink。reference-board 兼容路径将两字节地址发送与单字节读取分别作为单消息事务提交，通过 RT-Thread 可递归总线互斥锁保护每个地址/读取对。worker 在每个读事务对及每次字节写前等待 1 ms，并在地址与读取之间持该锁短暂等待 1 ms。地址/读取短传输最多尝试三次完整读取对，每次失败均记录日志，失败尝试不发布输出字节；互斥锁错误不重试。写入不自动重放。页回调拆为三字节的单字节写，每次尝试写后在总线锁外等待 10 ms，并逐字节读回再继续。短写、读重试耗尽或读回不一致立即终止，驱动成功不等于持久化成功。通用页边界、只读就绪检查及完整记录/seal 读回继续保留，不调用破坏性的 SDK AT24 probe。保守兼容模式牺牲吞吐：初始化 1 KiB 仅写周期等待就至少 10.24 秒，另加驱动和读回耗时；板验预留 60 秒。真实控制器行为仍由未修改 SDK 决定，已验证的实板范围见下文。seal 不与 body 或活动槽共享写页。

AIC_FORKLIFT_NVM_FILE_BACKEND 显式选择已挂载文件后端，AIC_FORKLIFT_NVM_FILE_PREFIX 和 AIC_FORKLIFT_NVM_FILE_KIND 指定路径和标识。reference-board 默认 EEPROM。挂载失败不格式化、不转用其他后端。首个 EEPROM 镜像运行前应确认新 1 KiB 布局和 WP；非空未知内容会被保留并报告。

## 结果与生命周期

LOADING 为异步启动操作。App 接收匹配 generation 后才允许设置动作。空白偏好区允许默认值通过普通可靠流程初始化。损坏或不兼容记录仍可使用 RAM 默认设置，但阻止写入；查询不初始化介质。

IN_PROGRESS 独占在途版本，直到匹配结果到达。保存 R11 时 App 已到 R12，R11 完成只推进 durable 到 R11，R12 仍 dirty。进入 I/O 后失败为 UNCERTAIN，保留 dirty，显式重扫协调后才能重试。协调不把旧介质值覆盖到新 RAM。barrier 针对明确目标版本，完整替代的更高版本可以满足目标，但不声称中间值精确写过。启动有效记录可满足对应 barrier。只支持 worker 派发前取消，不因超时删除阻塞 worker 或释放仍被使用的缓冲。

Host 退出前请求立即保存并有界等待；失败返回非零进程状态，不伪造 dirty 清除。板端 App 使用 meter_board_nvm_flush 和 meter_board_nvm_barrier；OTA 激活必须经 App 请求此门禁。当前 Protocol/App/UI 调度仍是既有 smoke 方案；本变更隔离存储 I/O，但没有关闭独立的生产 Runtime/Burst 门禁。

## 诊断与验证

使用以下原生 MSH 命令。每条修改命令必须领取结果后才能接纳下一条串口请求；QUEUED/APPLIED 不表示 DURABLE。

~~~text
meter storage
meter_settings language zh
meter_settings result
meter_settings units imperial
meter_settings result
meter_settings brightness 65
meter_settings result
meter_settings save
meter_settings result
meter storage
meter_settings retry
meter_settings result
~~~

Storage 输出 backend、state、dirty、RAM/inflight/durable revision、队列深度、底层错误、服务结果、降级状态与实际语言/亮度/单位。查询只渲染发布副本，不访问介质。主动重启前确认 DURABLE 且 dirty=0，重启后核对设置和序号。保存目标版本与异步命令结果分开。

Host 覆盖复用非活动槽时每一字节中断、身份/schema/CRC/seal 损坏、同代次冲突、pending 合并、迟到完成、时间回绕、最大防抖、序号耗尽、取消、协调，以及 EEPROM 跨页/WP/NACK/超时/sync 失败和真实文件重启。Demo SDL 重启验证中英文与单位恢复，损坏槽拒绝且不重写。执行 CMake/CTest；Linux 仍为默认 CI 平台。

2026-09-28 当前验收：仅应用层有界读恢复镜像的 BOARD_INIT、SETTINGS_SAVE、NOOP_SAVE、VALID_INIT_REJECT、SOFT_REBOOT_RESTORE 和 NORMAL_CAN_SAVE 均为 PASS。Platform 提交为 4fca8762243f9bb40cb753abd553f0808865cce0，并绑定已记录的脏源码快照；image SHA256 为 bf5ef8efa339376ffe83bd526201f0e62f7b30ff69c85a5608bf534a39ce3635。SDK 提交 9b78386dcaa54326487c8b208b2b7104d4b56c9b 的受跟踪目录树与基线 7608a666 相同。本地镜像清单保留完整构建身份、源码快照和原始 UART/CAN 证据；公开文档使用 reference-board 别名。

初始化至首条默认记录持久化的轮询观测耗时为 55.7 秒。地址 0x0207 的第一次地址传输失败，在最多三次读重试内恢复。初始累计错误来自旧布局损坏，最终底层错误为零。从第一条设置请求开始，中文、英制、亮度 65 在 15.3 秒内达到 durable revision 4，期间先后提交两个版本。重复相同设置及 save 不增加版本或写次数；已有有效记录时初始化被拒绝。

主动软件重启后，相同设置和 revision 4 加载为 READY，dirty=0、depth=0、errors=0、writes=0。READY 表示启动已加载有效记录，已满足对应 durable revision 门禁；DURABLE 表示运行期间成功提交，不是启动必需状态。首版测试脚本误要求 DURABLE，原始失败报告与按源码语义校正的结果同时保留。Host 回归覆盖 READY、payload/版本一致、门禁以及相同设置不重写。

使用既有正常 CAN 场景、标称 50 帧/秒时，两次追加保存分别在观测 9.4 和 6.9 秒达到 durable revision 5、6。Host 调度发送、原生接收、应用接收、Runtime 派发均为 890 帧；原生丢帧、应用错误及 Runtime overflow 均为零。速度保持 VALID，UART 诊断响应，UI present/flush 计数前进。这些观测不等于物理触摸或视觉验收。最终设置为中文、英制、亮度 65；COM 和 PCAN 已释放。

用户断电后重新上电，冷启动恢复为 PASS：固件身份一致，revision 6 与中文/英制/亮度 65 恢复，READY、dirty=0、errors=0、writes=0。写入过程中物理掉电与耐久测试仍为 NOT_RUN。逐字节传输保守且较慢，所列耗时包含命令与轮询开销，不是 EEPROM 裸吞吐。既有 CAN Burst FAIL 保留，正常负载测试不关闭生产 Runtime 或完整 HIL 门禁。Host 模拟不能单独证明硬件恢复。本工作是工程规范实践，不是授权 MISRA 认证。

保留历史失败：早期字节 transport 镜像（SHA256 0cf247370a4c9ba344435919a8df0b1f144d5eb70042b1f70a23ba96c2ef887e）初始化读回在 0x003C 失败，重扫在 0x0089 失败。实验性 SDK I2C 修复已完整撤回，对应镜像已撤销。当前应用层间隔和读重试通过上述限定范围板测，不宣称修复了 SDK 控制器实现。

首次部署遇到旧布局时，先导出备份，经布局授权后执行 meter_settings initialize CONFIRM。只有无有效槽、且上次 I/O 成功才允许；写入、同步和读回都在 NVM worker，拒绝清除有效槽。之后领取 meter_settings result，通过 meter storage 核对 DURABLE，初始化排队不等于完成。
