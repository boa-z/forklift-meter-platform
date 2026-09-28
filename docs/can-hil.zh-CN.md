# CAN HIL 自动化

## 范围与状态

首批目标是 reference-board 上的公开合成 reference-demo：CAN PHY、驱动、Runtime、解码器和 Domain。UI/触摸仍为独立人工验收。固件 Product 选择、RT-Thread CAN TX 和 Mixed 镜像完成验证前，Reference-Mixed HIL 保持 NOT_RUN。

HOST_PASS 表示解析器/向量/虚拟总线通过。HIL_NOT_RUN 表示硬件未执行或选定测试跳过/不完整。HIL_PASS 表示选定物理测试通过，完整 Gate 仍须查看用例名称。HIL_FAIL 保留失败结果。pytest 退出码为零但硬件跳过，不等于 HIL_PASS。

## 依赖与设计

tools/hil/requirements.txt 复用 cantools 40.7.1（MIT）、python-can 4.6.1（LGPL-3.0）、PyYAML 6.0.3（MIT）、pySerial 3.5（BSD-3-Clause）。新增 pytest 8.3.5（MIT）提供断言/JUnit，filelock 3.18.0（Unlicense）提供跨平台进程所有权。来源为 PyPI 发布包和上游项目。全部仅在 PC 运行，不增加固件 RAM/Flash。Windows 额外需要安装 PEAK PCAN-Basic，本项目不直接绑定 PEAK C API。

正常帧仅使用 cantools encode_message(strict=True)。测试描述物理值，不重复字节布局；错误 DLC 和未知 ID 可以直接构造 can.Message。通过元数据确定性生成可表示的 min、min+LSB、nominal、max-LSB、max、跨零有符号值及所有已定义枚举值。IEEE 浮点信号留待明确专门策略。向量在执行前保存。

DutSession 复用 pySerial 和现有串口命令发送函数。唯一读取线程持续保存原始字节，包括异步日志和非法 UTF-8。命令必须在超时内返回完整稳定字段，回显、ANSI 和提示符不算成功。每端口 filelock 和系统独占约束所有权，外部终端需先释放。不执行烧录或重启。

CanBus 将 I/O、通知与调度交给 python-can。RX ASC 保留 backend 原生时间戳，Host TX 记录接受或周期尝试时间，可能属于另一时钟原点；TX 日志或队列准入不证明 DUT 收到。使用原生 RX 间隔及带身份记录，不跨时钟原点相减。多个实例支持独立 bus，发送/监听失败会使测试失败。

## 命令

在 Platform 根目录运行。普通 pytest 执行 Host 工具并跳过实板。物理测试要求 --hil 和明确 UART 端口。缺 UART/CAN backend 时 SKIP 并记录 HIL_NOT_RUN；无响应或身份错误的 DUT 应失败。virtual CAN 不产生 HIL_PASS。SocketCAN 复用相同 API，网络接口由操作者预先配置。

~~~sh
python -m pip install -r tools/hil/requirements.txt
python -m pytest -q
python -m tools.hil.dbc products/demo/protocol/can/demo.dbc motion speed --nominal 25 --output evidence/speed-vectors.json
python -m pytest -m hil
python -m pytest -m hil --hil --can-interface pcan --can-channel PCAN_USBBUS1 --can-bitrate 500000 --dut-port COM11
python -m pytest -m hil --hil --can-interface socketcan --can-channel can0 --can-bitrate 500000 --dut-port /dev/ttyUSB0
~~~

仅连接目标测试板，停止 PCAN-View 发送并释放串口终端，按规则预约开发板。框架检查 Product、board、bitrate，清理 Trace 并注入合成流量。NVM/动态 TX 用例会临时改变亮度，在 finally 排队恢复；交接前确认恢复值已 durable。停止流量后信号可能 STALE，测试不烧录或重启。

--hil-evidence 指定证据根目录，--hil-image 记录已烧录镜像 SHA256，否则写 NOT_PROVIDED。可连接时始终记录固件报告身份。--dut-baud 默认 115200。--dut-board 指定必须精确匹配的板型身份（默认 reference-board），不会跳过身份校验。物理 HIL 不使用 pytest-xdist，UART 必须独占。

## Gate

| Gate | 判定依据 |
|---|---|
| HIL-01 | CAN RX 和 accepted/dispatched 增长；速度/SOC 数值正确且 VALID |
| HIL-02 | 速度/SOC 确定性边界向量；Domain 数值/状态 |
| HIL-03 | 停止超过 stale_ms 后恢复；转换计数及对应信号 Trace |
| HIL-04 | 未知 ID 使 unrouted 增加一次，正常解码继续 |
| HIL-05 | 已知 ID 的 DLC7 使 decode_failed 增加一次而非 malformed；INVALID_FRAME Trace |
| HIL-06 | 五帧各 10ms、持续 2s；至少 500 RX；无新增 overflow/drop/error/reset |
| Runtime periodic | 500 fps 负载下的原生 RX 间隔、抖动和长间隔 |
| Runtime NVM | UI/CAN 持续运行时设置达到 durable |
| Dynamic publication | 首允许 deadline 使用新语义版本，wire 完整性与 stale 恢复 |

reference-board 的 Burst Gate 额外保存 RT-Thread 原生 canstat 前后结果，并断言原生接收丢帧为零，从而发现 meter Runtime 边界之前的损失。burst-summary.json 分别记录 Host 周期发送尝试、原生驱动 RX/drop 和进入 Domain 路径的 RX/dispatched。canstat 扩展属于 RT-Thread，其他 DUT 前端复用本 Gate 前须提供等价查询。

计数使用增量而非整个运行期总值。合法短 CAN 帧属于解码错误；测试不注入物理坏 CRC 或 bus-off。动态 TX 与 NVM 补充原 speed/SOC 门禁，SDO/PDO 双总线和摄像头验收仍不在单总线套件内。

## 证据

显式 HIL 每次创建 evidence/hil/UTC-ID，包含 metadata.json、meter-info.txt、serial.log（原始字节）、can.asc、diag-before.txt、diag-after.txt、trace.txt 和 pytest 生成的 junit.xml。每个用例诊断/Trace、boundary-vectors.json、失败抓取提供细节。自定义 --junitxml 路径会记录。Evidence 被 Git 忽略，需单独保留。

元数据记录固件 Platform SHA、SDK identity、lvgl-aic revision、板卡、Host SHA、CAN 接口/通道/波特率、UART 端口、DBC SHA256、场景、镜像哈希可用性和测试各阶段结果。失败后独立尝试抓取 diag/runtime/can/domain/trace 并保存采集错误。中断的运行不能升级为 HIL_PASS。缺硬件时可能无法填充部分文件，原因明确保留。

python-can ASCReader 可读取生成证据，LogReader 支持后续 PCAN TRC fixture。未来回放应区分观测 RX 与注入 TX，正常合成场景仍从 DBC 编码。客户 DBC/trace 禁止进入公开仓库。本阶段不实现回放引擎或 CANopen server。

## 验证与限制

Linux CI 执行 pytest 和现有 Demo/Reference-B/Reference-Mixed CMake/CTest。Host 覆盖非法场景拒绝、全部 Demo 向量可表示性、枚举、UART 完整性/超时、原始字节、独占、双向虚拟 CAN、双通道隔离及失败证据，与实板结果分开。

YAML 定义 name、period_ms 和 message/signals；时序与断言采用 pytest Python，不新增 DSL。CAN1 需独立夹具和匹配的 Product 固件。TX 尝试时间不是硬件完成时间，有界 burst 不代表饱和资格验证。精确测试身份和结果见[验证记录](validation.zh-CN.md)。