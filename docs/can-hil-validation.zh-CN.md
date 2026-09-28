# CAN HIL 首次实板验证

## 基线与证据

以下板型名称为公开别名；原始串口证据在本地保留真实私有标识。

日期：2026-09-28。测试框架：98b56a9。板端固件 Platform：5aa2a20534c96975bfaaf31e8e42df6d6246ab50。SDK identity：2bc652c45fa8a0aa352538666c98ea1f86a85ec3-dirty-0164123894516e10。lvgl-aic：dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918。板卡：reference-board，LVGL 9.6.0，构建 Sep 28 2026 00:34:39。

操作者确认 PCAN_USBBUS1、COM11 连接正确并释放已有发送器/终端。Runner 使用 python-can PCAN 500000 bit/s 和 pySerial 115200 波特率。没有烧录、重启、参数修改、PCAN-View GUI 自动化或客户数据。结束后释放 UART/CAN，保留原板端固件。

最终原始证据目录：evidence/hil/20260928T004353Z-be4481e1（Git 忽略）。保存 metadata.json、junit.xml、serial.log、can.asc、构建身份、前后诊断、边界向量、各用例 Trace 和失败自动采集。未提供精确已烧录镜像哈希，image_sha256 为 NOT_PROVIDED。固件身份从实板读取，不使用当前 checkout 推断。

## Host 结果

本机 pytest：20 passed，默认跳过 6 项硬件测试。现有 Windows/MSYS2 CMake 构建和 CTest：Demo 32/32、Reference-B 23/23、Reference-Mixed 22/22。架构、public-clean、双语文档 Gate 通过。Linux CI 已在原三产品矩阵之前加入 pytest；最终结果须对推送的提交单独检查，不能用本机结果代替。

## 实板结果

| Gate | 结果 | 证据 |
|---|---|---|
| 正常解码 | PASS | CAN/runtime 计数增加，速度/SOC 正确且 VALID |
| 速度/SOC 边界 | PASS | 十组确定性向量和 Domain 断言 |
| 超时/恢复 | PASS | STALE 后恢复 VALID，转换计数和信号 Trace |
| 未知 ID | PASS | unrouted 恰好增加一次，正常数据继续工作 |
| 错误 DLC | PASS | decode_failed 增加一次、malformed 不变，INVALID_FRAME Trace |
| Burst | FAIL | 原生驱动丢帧，进入 Runtime 的帧数不足 |

整体判定：HIL_FAIL，不是 HIL_PASS。Reference-Mixed HIL：NOT_RUN。显示/触摸保留用户之前的人工结果，本次没有重新验证外观。

## Burst 发现

五条消息各按 10ms 调度。最终测量窗口中，Host 调度 1080 次 TX，原生 canstat 收到 1080 帧并报告新增 864 帧接收丢弃；meter Runtime 快照仅观察到新增 192 帧 RX/dispatched。包含查询和收尾的测量窗口约 2.259 秒。各快照边界不同，不能直接当作严格队列收支平衡式。

meter can drop 和 runtime overflow 仍为零。这些计数属于 Runtime 边界，没有暴露原生 CAN 接收 FIFO 丢帧，所以零值不能证明整条接收路径无丢失。原生驱动收到的 1080 帧与 Host 调度数一致，排除了该窗口由 Host 未达到发送负载导致失败。

源码显示 Demo smoke 循环先以预算 8 调用 meter_rtthread_adapter_poll，再执行 UI/LVGL 并休眠 16ms；接收排空因此依赖 UI 节奏。这是与实际丢帧一致的疑似瓶颈，仍需修改生产线程架构并测量新镜像，才能宣称修复有效。没有降低测试门槛把失败变成通过。

## 框架修正与下一 Gate

首轮捕获 Burst 失败，并自动保存要求的五类诊断。第二轮加入原生驱动证据，同时发现 UART 分片恰好停在 uptime 标签后，解析器提前接受响应。提交 98b56a9 要求字段完整换行且 uptime 为数字，并新增回归测试。最终运行没有 UART 收尾错误，仍能复现真实 Burst 失败。

下一步：通过 RT-Thread 原生 IPC 与单写者 Core 设计，使 Protocol 接收脱离 LVGL 节奏；补齐原生驱动 FIFO 丢帧诊断；构建新镜像后原门槛重跑六项 HIL。高度 6m 的生成浮点范围错误单独保留回归/修复。Mixed 固件及 CAN TX 未完成前，不宣称其 PDO/SDO 板测通过。复现命令见 [HIL 操作说明](can-hil.zh-CN.md)。
