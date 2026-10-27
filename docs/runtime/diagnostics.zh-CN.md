# 诊断与串口调试

## 职责与所有权

Log 用于立即阅读的重要事件，Diagnostics 用于当前状态与计数，Trace 用于近期结构化历史。UART 继续使用 RT-Thread Console；FinSH/MSH 负责命令分词；ULog 负责过滤、格式化与异步输出。不引入自研 Logger、Shell、UART 协议或打印队列。

平台无关 diagnostics 库只依赖公共 contracts。应用静态持有 diagnostics 并绑定 Core 和 Runtime；Protocol services 将可选指针传入 Adapter。调用者必须串行化全部修改与快照复制，未绑定时观测为空操作。Host 可在没有 RT-Thread、LVGL、SDK 的环境中复用接口。

生产端口用 RT-Thread 优先级互斥锁短时复制 owner 快照，MSH 复制后在发布锁外格式化并输出 UART，独立命令锁保护查询缓冲。查询不采样设备、不发送 CAN；LVGL owner 在 timer handler 后复制 Touch 缓存。

运行时线程独占 Core 与协议适配器；LVGL UI、CAN TX、NVM、Update 各在自己的线程。可移植 diagnostics 对象仍要求调用者串行访问；板端查询使用已发布副本，不并发读取活动 Core 状态。线程细节见 runtime-production.zh-CN.md。

## 计数与快照语义

Runtime 与板级 driver 计数描述不同边界。Runtime 记录 accepted/dispatched 和 reset；板级 CAN 诊断记录物理 bus RX、driver TX 成功/失败和原生 drop。队列准入与 driver completion 独立，均不证明远端应用接受。TX busy 是准入拒绝，本身不等同控制器错误；按明确观察窗口比较增量。

Domain 统计接受的 update，含数值未变化的 update；revision 只随真实 Domain 变化。来源与有效性转移独立计数，读取诊断或累计计数不改变 revision。参数、设置、故障元数据来自公共 Domain snapshot，不读取 Core 私有字段。signal 用 canonical key 查找并复制值，key/unit 借用不可变 Product catalog 的生命周期。age 用无符号毫秒差，适用范围为一个 uint32 时钟周期；UNKNOWN 明确显示，其数值 age 不能当作已收到数据的证明。


UI present_count 在展示入口统计，flush_count 来自 lvgl-aic；Touch 提供缓存计数和坐标。Storage 显示所选 backend 及 RAM/in-flight/durable revision，仅无后端时输出 unavailable。尚未实现 slow_frame；新增计数在 UINT32_MAX 饱和，原 Runtime 计数保持既有算术规则。

## Trace 与日志策略

静态 ring 含 256 条、每条 16 字节，共 4096 字节加 16 字节元数据。append 为 O(1)，无堆、无字符串格式化；snapshot 按插入顺序复制，跨 uint32 timestamp 回绕仍保持顺序；短缓冲选择最新记录。overwritten 饱和计数，clear 清除可见历史和覆盖数，保留消费者 sequence。查询不发送 CAN、不改车辆设置；唯一修改命令 trace clear 明确打印实际目标、数量与结果。

公共头文件定义 Runtime/CAN/Domain/Product event ID；模块编号 3、4（原 PDO/SDO）保留不复用。Product READY/FAILED 表示通用 Service 结果，不携带客户状态机。Domain 的 arg0 是 signal 身份。CAN 的 arg0 是 bus；其余细节以数字保留在 dump 输出中。

正常 CAN RX/TX、Touch sample、UI frame 只累计 Counter，状态转移与异常进入 Trace。RT-Thread 后端将选定事件映射到原生 ULog tag：meter.runtime、meter.can、meter.domain、meter.product；初始化使用 meter.boot/meter.board，LVGL 使用 meter.ui。无效帧与 CAN 错误为 WARN；连接变化与 product ready 为 INFO；product service 失败为 ERROR。重复告警每模块每秒最多一条，Counter 与 Trace 仍完整记录。Debug 遵循原生 ULog 设置，现有 SDK 使用 INFO 与异步 Console 输出。

后端每个 owner 循环消费增量 Trace。覆盖或显式 clear 可能丢掉尚未消费的日志事件；Trace/Log 是有界调试证据，不是可靠事件总线。消费前 clear 可能抑制对应历史 Log。MSH 输出可与异步 Log 交错，raw capture 保留真实字节。MSH trace 副本和日志消费者副本各占额外静态 4096 字节，总 trace 相关 BSS 约 12 KiB，不仅是历史 ring 的 4 KiB。

## MSH 查询

```text
meter info
meter diag
meter runtime
meter_exec
meter storage
meter can
meter can 1
meter domain
meter signal vehicle.speed
meter touch
meter trace
meter trace dump
meter trace clear
meter_color
meter_color ccm
meter_testcard
meter_testcard grid
meter_testcard patch 0x0DD462
```

未知或多余参数只显示 usage，不修改状态。CAN capture 预留为后续有界静态过滤缓冲（bus/id/count），没有无限实时 dump，也不在 RX callback 同步打印 UART。signal 不存在时明确报告；Demo 已知 key 为 vehicle.speed。info 输出 Product、platform SHA、SDK SHA、LVGL 版本、lvgl-aic revision、board、编译日期/时间和 uptime。

SCons 通过 tools/build_identity.py 自动生成身份。生成的头文件还会以 METER_BUILD_PRODUCT 记录选中的 Product（取清单 identity.id，否则取 meter_product_t.id），并以 METER_BUILD_PRODUCT_REVISION 记录其版本，因此归档的头文件本身就能说明镜像由哪个 Product 组成。干净仓库输出完整 SHA，dirty 仓库附加源码/状态指纹，包含未忽略的未跟踪源码；板名从 Kconfig 规范化。源码身份与镜像 SHA256 共同绑定证据，只有日期不足以识别镜像。仓库不可访问时显示 unavailable。SDK 可能因其他私有子模块脏而带标记，不得为了去掉标记清理其他任务的文件。

## 显示色彩台架

`meter_color` 与 `meter_testcard` 是面板台架工具，不属于生产诊断。gamma/CCM 由显示引擎持有：色卡只提供参考像素，
`meter_color` 的补偿对色卡同样生效，与 Product 界面完全一致。

`meter_color` 打印并修改显示引擎的 gamma、CCM 表（`meter_color ccm`、`meter_color ccm r|g|b C0 C1 C2 OFF`、
`reset`、`identity`、`sat`）。改动只存在 RAM，断电即丢失。开机默认 CCM 表来自
`AIC_FORKLIFT_DISPLAY_CCM`，`reset` 复原到它，`identity` 清成单位阵。

`meter_testcard` 在 `lv_layer_top()` 上画诊断色卡；Product 界面照常在下面运行，`off` 后原样返回；色卡打开时
触摸由色卡吸收。MSH 命令只写一个请求，由 UI owner 线程用 LVGL 定时器取走执行，shell 线程不直接碰 LVGL。

| 命令 | 结果 |
|---|---|
| `meter_testcard` | 当前状态、预置色表（序号与 `0xRRGGBB`）和用法 |
| `meter_testcard grid` | 网格总览，每页 12 块；`next`/`prev` 翻页 |
| `meter_testcard patch 0xRRGGBB` | 指定任意颜色的整屏色块 |
| `meter_testcard patch INDEX` | 按预置序号显示整屏色块 |
| `meter_testcard next` / `prev` | 网格翻页；单色切换下一个/上一个预置色 |
| `meter_testcard off` | 关闭色卡 |

内置预置为灰阶、RGB、CMY；Product 可在 `meter_ui_factory_t.create()` 里用 `meter_testcard_set_extra()`
追加自己的 UI 关键色。不带参数的命令会打印整张预置表，主机脚本可按序号选色，让每次拍照顺序一致。
上位机 `tools/serial/meter_ccm_tuner.py` 用同一个串口同时驱动两条命令：CCM 按行滑杆调，
「测试色卡」区可直接开网格、翻预置色、显示单色与关闭色卡。

单色模式在固定 `#202020` 背景上居中画 65% × 62.5% 的色块（800 × 480 屏为 520 × 300），块内没有文字、边框、
阴影和透明层；同框保留 100 × 100 的 `#808080` 灰参考块。色值、序号和提示都在两个采样区之外。拍照请在正常环境光
下用同一台手机、同一曝光档位比较；手机自动曝光仍会改变结果。

## Host 采集与证据

```sh
python -m pip install -r tools/serial/requirements.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 capture evidence/serial.log
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter info" --output evidence/info.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter diag" --output evidence/diag.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter runtime" --output evidence/runtime.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter can" --output evidence/can.txt
python tools/serial/meter_serial.py --port /dev/ttyUSB0 command "meter trace dump" --timeout 10 --idle 1 --output evidence/trace.txt
```

Windows 将 /dev/ttyUSB0 替换为明确的 COM 口。安装依赖与运行工具使用同一 Python 解释器。文件追加原始字节，保留 ANSI 与非法 UTF-8，不解读协议成功与否。capture 直到 Ctrl-C 或 --duration；command 收到数据后等待 --idle 静默，最长 --timeout，无响应退出失败。长 dump 应增加超时，只有命令回显不等于执行成功。打开设备前设置控制线为 inactive，但 DTR/RTS 连到 reset 时仍需注意适配器与 OS 行为。

同一板端串口只能由一个进程持有：先停止 capture，再逐个执行 command，或使用能记录整个会话的外部终端。不要并发运行示例的 capture 与 command。采集 boot 时先启动 capture，再由操作者复位开发板，保存完整 boot 和每条命令响应。工具不会隐式烧录或重启。

烧录前按 SDK 项目管理规则预约共享开发板并记录恢复镜像。将精确的生成镜像复制到 evidence，保存 SHA256（PowerShell Get-FileHash 或 Linux sha256sum）、生成身份头、.config、image/map/ELF 与 SDK/platform revision。之后从实板获得 boot log、info.txt、diag.txt、runtime.txt、can.txt、trace.txt；还需查 domain、已知 signal、touch，在相应 Mixed 镜像运行 CAN fixture。禁止从 Host fixture 伪造板端日志。

## 验证与限制

HOST_PASS 要求纯框架构建、架构、diagnostics、trace、查询格式、生产 MSH 函数体、串口工具与构建身份测试通过。MSH Host 测试只替换原生 RT API/注册，并断言输出发生在 owner 锁外，还验证原生 ULog 等级、告警限频和正常流量 流量静默；串口测试包含 pyserial loopback，两者均不能证明物理 UART 正常。Linux 为默认 CI。

IMAGE_READY 要求当前 SDK 固件构建与产物哈希；BOARD_PASS 还须与该镜像绑定的物理 boot/MSH/fixture 证据。没有可用板端串口时报告 BOARD_NOT_RUN。不提供持久化 trace、coredump、网络日志、自研 UART parser 或无关 Demo 页面。

相邻架构见 [RT-Thread adapter](../../platform/rtthread/README.zh-CN.md) 与[协议契约](protocols.zh-CN.md)。
