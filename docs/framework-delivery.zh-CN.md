# Runtime、存储与升级交付

## 基线与依据

本次执行用户批准的 2026-09-28 开发任务书和治理补充。两者 SHA256 分别为 B546D42269F26DD736A5DD0A08481EAF0B36B53C68009D8C3CE72BE133E46C21 和 B072FBE8E1F573A3CD7260EEB3A64DE4C3FCD5C95E669A2F9A60F51590E3F240。原件与源码证据保存在忽略目录 evidence/framework-baseline。这是规范，不是验收证据。

开工 Platform 为 ec3f087cd8406fb2d374828e28c5fd3a7117f2f6，SDK 为 7608a666f047783c813438d40494c65479e77788。历史整理前 beb680e 的 Platform 文件树与之相同。开发使用 codex/runtime-nvm-ota；main 合并须经独立评审。烧录有明确身份的新镜像前，实板仍是之前报告的 5aa2a205 构建。原 Demo 六项 HIL 保留五项 PASS、Burst FAIL；Mixed HIL 为 NOT_RUN。

## 决策与门禁

| 决策 | 来源与处理 | 剩余门禁 |
|---|---|---|
| D-01 构建 | SDK 配置：1 MiB SRAM、16 MiB PSRAM、1000 Hz tick、32 优先级；源码哈希本地保留 | 捕获实际目标编译命令、ELF/MAP、编译器模型及实测预算 |
| D-02 EEPROM | Board 头标记 BL24C512A、i2c0、地址 0x50、64 KiB、128 字节页 | 写入前确认实物、WP、时序及新布局授权 |
| D-03 布局 | SDK pack 声明 4 MiB os/os_r、14 MiB rodata/rodata_r、冗余 ENV、独立 data/data_r | 核对运行布局、镜像余量及可恢复首装；目前不写 Flash |
| D-04 传输 | 新任务明确 UDS + ISO-TP；审查固定版本 iso14229，保留 SDK OTA/ENV | 完成协议与后端审查及实板闭环 |
| D-05 可信性 | Product/Hardware、长度和完整 SHA256；hash 不是签名 | 签名、密钥和 anti-rollback 属于后续 Security Framework |
| D-06 时序 | 合成 0x3C0/50 ms 与 0x2F0/100 ms；报告分布 | 产品抖动、维护及准入策略未确认 |
| D-07 记录 | Demo 偏好与明确本机权威的合成参数共用 MSP2 记录；其他 Product 显式选择 | 远程参数不得隐式获得本机持久化权威 |
| D-08 试运行 | 当前 SDK 挂载提前确认状态，激活忽略 flush 结果 | OTA HIL 前分离 finalize/activate/health-confirm 并验证恢复 |

本次 CAN OTA 任务取代早期 SDO 传输设想。SDK UDS 请求长度校验、回调中阻塞写入和分块计数需要替换；成熟 UDS/ISO-TP 库负责传输，独立 Update worker 复用 ArtInChip 安装流程。激活必须等待 NVM durable barrier，传输完成不代表安装或升级成功。

## 所有权与执行

只有 App 修改 Domain 与 Product 工作流。Protocol 先完整解码同帧语义批次，容量不足时整批拒绝。UI 独占 LVGL 并读取复制后的数组。App working、published 与 UI-local 使用三份独立、由 Product 决定容量的存储。深复制在平台发布 mutex 内同步有界执行，先校验容量与别名再写任何目标；复制函数本身不加锁。

Protocol 独占协议栈与 TX ticket；每总线 TX worker 独占 SDK write。NVM/Update worker 独占各自后端状态与阻塞 I/O。接纳请求前预留终态空间，领取确认后才释放。QUEUED、APPLIED、DURABLE、DRIVER_COMPLETED 与远端响应含义不同。session/generation 隔离本地旧完成，不能识别全部线上 SDO 迟到响应。

所有 owner 通过 RT-Thread 原生 MQ/event/mutex 通信。纯 C step 函数只有一个 owner，不隐含线程安全。worker 消息持有值或有界池句柄，不借用生产者栈。in-driver/in-I/O 请求不能因本地超时而回收；停止失败保留资源。

## 工作包与验收

| 工作包 | 交付 | 开工状态 |
|---|---|---|
| W0/G0 | 固定证据、有限决策、来源分类及检查计划 | IN_PROGRESS；硬件及授权标准/工具输入待补 |
| W1 | 深复制、完整批次、类型化请求/结果及服务契约与 Host 测试 | IN_PROGRESS |
| W2 | Protocol/App/UI、TX worker、周期 TX、异步 SDO、安全诊断 | NOT_IMPLEMENTED；原 Burst 仍 FAIL |
| W3/G1 | 记录 codec、版本/barrier、双槽、EEPROM/File 后端与独立 worker | HOST_PASS / BUILD_PASS；BOARD_NOT_RUN / POWER_CUT_NOT_RUN |
| W4 | UDS + ISO-TP 传输、有界块、manifest 与 Host 下载工具 | NOT_IMPLEMENTED |
| W5 | SDK EEPROM/OTA/ENV 修复、候选及试运行确认 | NOT_IMPLEMENTED |
| W6/G4 | 联合固件、原 HIL 加存储/升级恢复、合规闭合 | NOT_RUN |

各后端分别报告 Host、BUILD、BOARD、POWER_CUT。源码检查点不等于 SOFTWARE_COMPLETE 或 COMPLETE_ACCEPTED。Host 通过不自动授权物理写入或断电。保留原 HIL 输入及门槛，不隐藏原生 CAN FIFO 丢弃。

## 治理

第一方 C（包括 AI 输出）按 native 管理。确定性生成代码保留输入/工具身份并参与分析。RT-Thread、LVGL、CANopenNode、SDK 是须保留版本、许可证和边界证据的 adopted 组件；自主 lvgl-aic 逻辑不自动成为第三方。Host-only 工具适用独立质量范围。

用户确认无授权标准或正式检查器，按自有工程规范继续：保持单一 owner、显式错误、边界校验、中文注释与现有 CI。不声称正式 MISRA 合规认证；第三方边界和未验证硬件能力明确记录。

## 证据与下一门禁

阶段报告绑定实际源码、测试、限制与下一工作包。证据保存在本地 evidence/，不进入公开源码。物理产物绑定实际烧录 image SHA256 和运行身份。未知器件几何、公钥部署、恢复或许可不能用示例常量替代。
