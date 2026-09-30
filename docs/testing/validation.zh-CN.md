# 固件验证记录

## 交付范围

日期：2026-09-28。基线：ac3dc01。原验证分支：codex/dynamic-periodic-tx。Firmware 源码：3c141ff479e40a2b7d21f2873d7ccb0c05b939aa。Host UART 工具：4793bb48ff72056a2a1e18352d6287bccb693c51。开发板运行 dynamic-tx-e。未修改 SDK 源码、持久配置或父仓库 gitlink。

现行契约见[Runtime](../runtime/runtime-production.zh-CN.md)、[Dynamic TX](../runtime/dynamic-periodic-tx.zh-CN.md)与[治理](../compliance/status.zh-CN.md)。本页保存历史测试身份，分支名和原始 SHA 不是对当前 main/板上软件同一性的声明。旧阶段报告可从 Git 历史检索。

## 验证结果

本地 CTest：Demo 44/44、Reference-B 34/34、Reference-Mixed 33/33、headless OTA 42/42。Python：97 passed、12 skipped；实板 HIL 独立执行。Firmware 源码 3c141ff（run 36430288416）与 Host 工具源码 4793bb4（run 36430815503）的 Linux host、quality 均通过。Firmware 对应 quality 包含 ASan/UBSan 42/42、原分析器、七个生成生产翻译单元的两种分析器检查，以及 31 秒内 423074 次 fuzz 执行。未降低任何门槛或检查。

最终单总线 HIL：30.47 秒内 9/9 通过。500 fps 负载/burst 测试记录 1052 个 scheduled 帧、1052 个 native driver RX 帧，无 driver drop 或 Runtime overflow；dispatched 计数采用自身观察窗口。NVM/UI 共存、freshness 停止/恢复、动态版本选择及原有 Runtime 回归通过。249 个动态 TX 帧通过字节补码/XOR 检查。亮度 31/47/63 对应 revision 20/21/22，取得版本后分别 22/11/7 ms 的首个允许 deadline 使用相应新版本。

| 测量项 | Critical 50 ms 帧 | Ordinary 100 ms 帧 |
|---|---|---|
| 周期间隔样本 | 125 | 62 |
| 平均间隔 | 49.9971129 ms | 99.9985246 ms |
| 最小间隔 | 49.195 ms | 99.201 ms |
| 最大间隔 | 50.987 ms | 101.036 ms |
| 最大绝对抖动 | 0.987 ms | 1.036 ms |
| 长间隔 | 0 | 0 |

选定首 deadline 样本的 scheduler lateness、queue residence、driver duration 均为 0 ms，诊断分辨率为 1 ms，不代表物理延迟为零。PCAN RX 保留原生时钟，Host TX wall time 使用另一时间原点。按身份及日志顺序窗口关联，只在原生 RX 时钟内计算间隔，不直接相减两个时钟原点。这些是实测结果，不是新增时间指标。

## NVM 与 OTA 回归

原 NVM 优先级 24 会使重复 EEPROM I/O 唤醒等待优先级 22 的 UI。调整 NVM 为 21，仍低于 App/Protocol/TX。观察到单次提交由 8.951 s 改善到 3.865 s，未改变逐字节写入/读回或持久化布局，原有 12 s HIL 门槛不变。最终只读检查为 DURABLE、dirty=0、errors=0，RAM/durable revision 均为 54；保留当前亮度 10。

dynamic-tx-d 到 dynamic-tx-e 的 OTA 在 500 fps 背景流量下通过：1098752 bytes 用时 101.4973777 s，吞吐 10825.42 B/s（10.83 kB/s）。1024-byte 主动 Abort 后恢复 normal，随后完整传输、安装、激活、重启并确认新固件。亮度 75、language 1、imperial 1 跨升级保留。限定维护窗口捕获 1655 个 critical 帧、零 ordinary 帧；critical 平均间隔 50.000049 ms，范围 49.067020–50.987005 ms。测试会话断开期间的历史 no-ACK 错误不伪装为整个运行期零错误。

## 产物身份与证据

| 产物 | 身份 |
|---|---|
| 测试镜像 build-e/*.img | SHA256 1c1481f1a8996876f47eb1e99010a032c7754fba72f0852295fbebc215f862ab |
| os.itb | SHA256 21f984729b04c6cff47c9c2c3e6f4d868e96c1eef010c45f511e89cffe61ed46 |
| package-e/ota.cpio | SHA256 5da9af4fe5190c3f69fe7a7b6199d4e16330946095fd6753203550ccdfe76cf6 |
| 已恢复 SDK .config | SHA256 2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241 |

本地证据位于 evidence/dynamic-tx/：build-e、package-e、install-e-retry、hil-e/20260928T134536Z-8b4376dc、final-state 和 ci-quality。原始 CAN/UART、JSON 诊断、JUnit 及前期失败记录均保留在本地，不作为公开仓库 fixture。最终镜像 1800704 bytes；ELF text/data/bss 为 1082908/12716/214560 bytes。增量 target capture 包含 47 条实际编译命令，不等同完整 target compilation database。

## 治理与限制

Tool Quality Green 由上述精确 CI run 支持。Project Governance Conforming 仍为 PENDING：TAD-001 需要人工适用性批准，main 缺少 required-check 强制执行（检查时 protection HTTP 404、rulesets 为空）。Formal MISRA Compliance 为 NOT ASSESSED。采用的 target LVGL adapter 许可证证据也待评审，不隐含 Agent 批准。

原 portable 分析器覆盖 core/runtime/storage/update，header filter 包含 contracts；补充分析覆盖七个生成源文件。手写 Product/platform/UI 翻译单元及 target-specific analysis 尚未全面覆盖。Target GCC 为 Xuantie 10.2.0，newlib 3.2.0，rv32imafdcpzpsfoperand_xtheade/ilp32d、O2；未捕获显式 C 标准参数。Linux 工具版本与依赖 pin 单独记录。

Catalog 调整字段顺序以消除 padding finding；生成代码与仓库内初始化改为指定字段。使用 positional initializer 的 External Product 需要迁移并重新编译，显式持久记录编码不变。第一版周期契约采用 uint32 wire-state token；诊断与 wire identity 暴露有界低位，内部 revision 仍为 64 位。

仅验证一个物理 CAN 接口，不新增同时多总线、rollback、secure update 或断电恢复支持声明。待闭合项为人工适用性/许可证评审、管理员 required-check 配置、按实际范围扩展 target analyzer，以及多总线夹具。未新增 Demo 页面或重构 SDK。

## 先前停机验证

固件 runtime-production-j（368eed1c346ec87912d4b2c40d366ca0cd1f915f）在下载 4096 bytes、设置待持久化时执行协作停机，2.768 s 后 Update ABORTED、NVM durable revision 23、CAN 关闭、UI 释放且新设置被拒绝。重启恢复亮度 70，再由测试恢复 75。证据：evidence/runtime-production/shutdown-j3。此结果属于该历史镜像，不重新标记为 dynamic-tx-e 的完整停机故障矩阵验证。

## Application 服务实板验证

源码 5cfa0bfeefa7e97dccb56319986af7704673a9bf，固件 services-a，且只包含一个 Demo Product。已在连接的实板测试，公开身份 reference-board，COM11 与 PCAN_USBBUS1，500 kbit/s；SDK 交接记录明确实际板型。记录 20260928T171051Z-dd46a0a2 对应 UTC 9 月 28 日 / 本地 9 月 29 日。这是新实板证据，不复用旧 dynamic-tx-e 结果。

| 检查 | 结果 / 范围 |
|---|---|
| 原生 SCons 构建 | PASS；Xuantie GCC 10.2.0 V2.6.1、SDK Python 3.8.10；原配置逐字节恢复 |
| 构建期组合 | 链接 Demo composition 和 projection；未选入 Reference-B/Mixed 源码；其他 Product 仍独立 Host 构建 |
| 既有 CAN OTA | 验证 1102848 字节包，激活并重启；UART 与 CAN 身份匹配源码 5cfa0bf / services-a |
| 物理 HIL | 9/9 PASS；原门槛覆盖接收/解码/过期恢复、未知 ID、DLC 拒绝、突发负载、周期时序、负载下 NVM 及动态发布 |
| 突发观测 | 原生 RX 1051、原生 drop 增量 0；Domain 路径 RX/派发 1015；门槛统计无新增错误/溢出/重置 |
| 周期观测 | 50 ms 均值 49.9935、最大绝对抖动 0.9420 ms；100 ms 均值 99.9878、抖动 1.0740 ms；未观察到长间隙 |
| 交接 | RUNNING/NORMAL；update IDLE、维护关闭；EEPROM DURABLE、dirty 0、RAM/耐久 revision 60、亮度恢复 10；UART/CAN 已释放 |

周期数据只是一个测量窗口，不是车辆容差或最坏情况保证。CAN 生命周期错误包括无 Host ACK 端点的间隔；HIL 仍按原窗口增量断言。观察到 UI flush 进展，但不宣称视觉/触摸验收或照片。新增标定/profile 映射仍是可选合成测试，不是真实控制器标定。掉电、多总线及私有 Product 集成均 NOT_RUN。

| 产物 | SHA256 |
|---|---|
| 完整 build-a image，未整包烧录 | 276fb0dc4fcba9e92ab7ffa132cf09de2d076990a1c0a6b2aa0731956c427426 |
| 构建 d13x_os.itb | 9137f6a0178b12fc2282e6469d84e8b589e815499f27b035b79c6276bc2d8fcd |
| 实际安装 package-a/ota.cpio | 3ce1457a2d318c2cebb8c00d248961bc4d9c9470b12821e5f419aa95c750eb70 |
| 恢复的 SDK .config | 2494d4e32238755e8fe44f9bfc02cb034be6b2d1780fa2e87cc9ef653c3cf241 |

证据根为 evidence/services/：build-a、package-a、install-a、hil/20260928T171051Z-dd46a0a2 和 handover。sha256.json 索引原始 UART/CAN、JUnit、配置、镜像及 JSON。HIL image_sha256 指实际安装 CPIO 包，不是完整 image。OTA 经既有 native auto-confirm 路径更换 OS 槽；未改变 bootloader 和信任策略。

SDK 身份为 57777e0b5d027e030de1daf0799f0c05102953d9-dirty-378865321b7708a1，已归档补丁/状态及精确依赖。LVGL 9.6.0，lvgl-aic dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918。递归依赖清单暴露既有 iso14229 文档子模块缺失 .gitmodules 映射；保留部分输出/错误和非递归 pin，未静默修复。目标捕获包含 71 条实际增量编译命令，不是完整目标数据库。ELF text/data/bss 为 1083436/12716/214624 字节。SDK 原有探测/pywin32 警告保留在原始构建日志中。不因此宣称完整目标分析器或当前 HEAD Linux CI 验收。
