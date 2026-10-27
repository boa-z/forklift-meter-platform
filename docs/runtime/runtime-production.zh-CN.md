# 生产运行时

> [English](runtime-production.md)

## 线程

一个运行时线程独占 Domain（Core）、协议适配器和全部 Product 回调。只有可能阻塞在硬件上的调用才放在单独的线程里。

| 线程 | 拥有 / 负责 | 唤醒条件 | 栈 |
|---|---|---|---|
| `meter_app`，优先级 19 | Core、协议路由与适配器、Product 的 `app_run`/`app_tick`/`local_action`、周期帧调度、NVM 记账、UDS 服务轮询、快照发布 | CAN 接收、UI 动作，或 5 ms（链接 CAN 升级端点时 1 ms） | 12288 字节 |
| `meter_tx0` / `meter_tx1`，优先级 18 | 每条已打开的总线一个；唯一调用 CAN 驱动写入的线程 | 自己的队列 | 各 2048 字节 |
| `meter_ui`，LVGL 优先级 + 2 | LVGL、Product UI | 16 ms（显示升级页时 50 ms） | 默认 32768 字节 |
| `meter_nvm`，优先级 21 | 阻塞的 EEPROM 或文件 I/O | 自己的请求队列 | 4096 字节 |
| `meter_upd`，优先级 25 | 升级候选的 Flash 写入 | 自己的任务队列 | 12288 字节 |
| `meter_wdog`，优先级 26 | 喂硬件看门狗 | 200 ms | 2048 字节 |

Product 代码不需要加锁：除 UI 工厂外，所有回调都在运行时线程里依次执行。主机模拟器用 `meter_runtime_step()` 在一个循环里跑同样的顺序。

## 运行时线程的一轮

1. 轮询升级端点；进入或退出维护会开始新的会话（见下文）。
2. 从已打开的总线读至多 64 帧。UDS 帧交给升级端点，其余帧立即路由并解码，解码直接写入 Core。
3. 应用排队的 UI/MSH 动作（`METER_ACTION_PRODUCT` 交给 `local_action`，其余交给 Core）。
4. `meter_runtime_step()`：适配器 `process`、带命令端口的 Product `app_run`、信号超时、Product `app_tick`。
5. 用当前快照编码到期的周期帧并入队。
6. 把亮度设置写到背光，把变化的设置交给 NVM，领取 NVM 结果。
7. 发布：快照 revision 变化时复制给 UI，并复制诊断数据给 MSH。

## 线程之间的接口

| 从 → 到 | 机制 | 深度 |
|---|---|---|
| UI / MSH → 运行时 | `meter_execution_action()`；只有队列满或当前不接收设置时返回 false | 8 个动作 |
| 运行时 → UI | `meter_execution_present()`；revision 变化时 UI 才在发布锁内复制 | 一份发布副本 |
| 运行时 → 发送 | 普通队列（协议帧与周期帧）和紧急队列（UDS）；连续最多 4 个紧急帧后让出一次普通帧 | 每条总线 16 + 16 |
| 运行时 ↔ NVM、升级 | 各自的请求/结果队列 | 在途 1 个 |

发布锁（`meter_debug_lock`）同时保护公共诊断，所以 `meter domain` 与 `meter signal` 读到的和 UI 是同一份副本。

## 命令

`meter_command_port_t.send()` 按 Product 的 `command_route` 把命令交给所属适配器的 `command()`，在同一次调用里返回适配器是否接受。需要等待应答的事务（SDO、参数读写、版本查询）由适配器自己负责：在 `process` 里发帧、在 `on_frame` 里匹配应答、自己处理超时，结果以信号发布。

## 周期帧

`meter_periodic_frame_t` 给出总线、标识、长度和周期；期限到达时 `encode(snapshot, now, frame)` 用当前快照填写数据。是否新鲜由编码器按各信号的状态自己判断。每个条目最多一帧在发送队列里等待；一个周期内还没交给驱动的帧被丢弃，所以总线无应答时不会积压。迟到的期限跳过错过的周期，不补发。`critical` 条目在升级维护期间继续发送。

## 会话与维护

开机以及每次进入或退出升级维护（`update_exclusive`、打开的 UDS 编程会话或 `meter_update maintenance on`）都会开始新的会话：推进代次、把有效信号标为过期、复位适配器并调用 Product 的 `app_reset`。上一个会话里排队的帧在交给驱动之前被丢弃；UDS 帧不属于会话。维护期间收到的车辆帧只计数不解码，跳过 `app_run` 与适配器 `process`，只发送关键周期帧，拒绝设置，UI 显示升级视图。没有降级模式：发送队列满只增加 `queue_full` 计数。

## 启动

UI 线程组装 Product，初始化 LVGL、Core 与显示，创建 Product UI，然后启动运行时线程。运行时线程先等存储的设置读完（打开总线前必须知道 CAN 速率），再只打开 Product 用到的总线（路由、周期帧、升级端点），启动它们的发送线程和升级端点，开始第一个会话。启动失败会记录日志，运行时不启动；看门狗只在成功启动后才监督。运行时没有停止流程：掉电时的数据可靠性由设置记录格式保证，升级激活前会等待设置写入介质。

## 诊断

`meter_exec` 打印代次、维护状态、被拒的动作、被抑制的帧、发送队列满次数、每条总线的发送成功/失败/丢弃计数，以及每个周期条目错过的周期和最大迟到。`meter can`、`meter runtime`、`meter domain`、`meter storage` 与 `meter trace` 见[诊断](diagnostics.zh-CN.md)。

## 构建期 Product 选择

每个固件镜像只包含一个 Product，SCons 按 defconfig 的 `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT` 选择；环境变量 `METER_PRODUCT_ROOT` 可在单次构建中覆盖。选择为空时固件构建停止。主机 CMake 在 `products/demo` 存在时默认选它，`-DMETER_PRODUCT_ROOT=` 只构建框架。所选 `product/sources.json` 提供固件组装（`contracts/meter_firmware.h`）：三份互不重叠的存储（运行时 Domain、发布副本、UI 副本）以及语言初始化。
