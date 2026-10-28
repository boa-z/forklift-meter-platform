# 复用 LVGL 与 SDK 服务

> [English](reuse.md)

本次优化以现有 Luban-Lite SDK 1.3.2 与 Framework 为基线，复用固定版本的 LVGL、RT-Thread 和 ArtInChip 服务。产品功能与 UI 通过 Product 接入这一组合，不以另一套 SDK 的接口作为前置条件。只有成熟接口能保持现有契约时，替换才有收益。设备访问属于 platform/BSP，客户措辞、协议和策略属于 Product，contracts/Core 保持独立于 LVGL 与 RT-Thread。

## 已落实的精简

- 可视控件（仪表盘、指针、环形/弧形条、线性表、状态指示、数值标签、升级页）、它们的数值格式化/映射助手以及控件词汇（`meter.*` 状态词）移入唯一的使用者 Demo。框架是多 Product 的运行平台，不是仪表控件库：`ui/common/` 只保留翻译服务和显示测试卡，架构门禁拒绝其他内容。
- 带类型的整数占位符改用 libc `snprintf()` 和固定的 `PRId32`/`PRIu32` 格式串。翻译模板仍按数据解析，保留参数重排、花括号转义和有界输出。
- 删除始终返回 unsupported 的 OTA 后端及其自证测试。只有该测试链接它；固件早已在 `SConscript` 中显式选择 `meter_update_backend_aic.c`。同时删除原生后端未使用的 context 句柄。保留 Update 服务、UDS 和取消测试；它们不验证 SDK 安装器。
- 看门狗诊断用 RT-Thread 调度临界区直接复制权威状态，删除五个未同步的计数副本；状态推进使用相同保护。当前单核临界区内只有有界计算/复制，日志与设备调用都在区外。

## 按源码决定保留的内容

下表中 SDK 路径相对 SDK 根目录，Framework 路径相对本仓库。

| 模块与源码 | 已复用的成熟接口 | 剩余代码保留理由 |
|---|---|---|
| 国际化：`ui/common/i18n/meter_i18n_runtime.c`；`ui/common/i18n/lv_i18n` | lv_i18n 语言包、当前语言、复数规则 | LVGL 9.1 没有 `lv_translation`。查表由 lv_i18n 提供；标签绑定、语言变化事件、显式语言查询、Product 拥有的持久化语言 ID 与字体角色仍由薄适配层负责。 |
| 显示：`platform/rtthread/meter_board_port.c`；SDK `bsp/artinchip/drv/display/` | `mpp_fb_ioctl()`、`lv_aic_init()` | 显示层归属、额外 UI 矩形、alpha 和色键属于应用组合；有摄像头的 Product 可关闭独占。无需重做 framebuffer/渲染驱动。 |
| 背光：`platform/rtthread/meter_board_settings.c`；SDK `bsp/artinchip/drv/display/panel/panel_com.c` | `rt_pwm_set()`、`rt_pwm_enable()`、`PWM_CMD_GET` | 面板助手只按固定周期和编译期占空比使能，不提供持久化亮度、配置底座、读回与重试。保留现有原生 PWM 适配及板级 Kconfig 参数。 |
| CAN：`platform/rtthread/meter_board_port.c`、`meter_execution_port.c`；SDK `kernel/rt-thread/components/drivers/can/can.c` | `rt_device_read/write/control()`、RX 通知、CAN 状态 | 驱动 TX 会等待邮箱/完成事件。保留每总线 worker，避免一条总线阻塞 Core、UI 或另一条总线。会话过期和 OTA/普通流量的有界公平性也不是原始驱动队列提供的能力。 |
| IPC：`meter_execution_port.c`、`meter_nvm_port.c`、`meter_update_port.c`；SDK `kernel/rt-thread/src/ipc.c` | 原生消息队列、信号量、事件、互斥锁 | 队列复制请求/结果，单一运行时 owner 修改 Core，发布快照在互斥锁下复制。引入事件总线或跨线程共享可变 Core 会增加复杂度。 |
| RTC：`platform/rtthread/meter_rtc_port.c`、`runtime/meter_wall_clock.c`；SDK RTC 驱动与 libc `ctime.c` | `time()`、`gmtime_r()`、`set_timestamp()` | UTC 绑定、无效时钟过滤和 UI owner 的读缓存仍有用途。纯 64 位日历换算避免时区依赖及平台有符号 32 位写入范围的限制。 |
| 看门狗：`runtime/meter_watchdog.c`、`platform/rtthread/meter_watchdog_port.c`；SDK `bsp/artinchip/drv/wdt/drv_wdt.c` | RT-Thread timeout/start/keepalive 控制 | 设备负责复位 CPU，不判断 runtime/UI 是否健康推进。保留 owner 监督与启动门禁；改为空闲钩子喂狗会掩盖 UI 停滞。 |
| OTA：`update/`、`platform/rtthread/meter_update_backend_aic.c`；SDK `packages/artinchip/ota/` 与 `packages/artinchip/env/` | iso14229/ISO-TP、mbedTLS SHA-256、SDK 下载/烧写/ENV 接口 | SDK ENV 在 CRC 失败后可能返回默认值，激活接口也不完整传递 flush 失败。保留独立 ENV/几何校验、有界 OS-only 包解析、Flash 读回、NVM 持久化屏障和 Product 准入。 |

## 当前 SDK 的复用边界与验收

以下取舍以当前 SDK 的实际源码为依据：

- `packages/artinchip/ota/burn.h` 和 `burn.c` 已提供 `aic_ota_part_read()`，现有后端直接调用它完成 Flash 读回；不存在需要等待另一套 SDK 补齐的接口缺口。保留该调用，不另写 MTD 读取兼容层。
- `kernel/rt-thread/components/drivers/can/can.c` 会等待发送邮箱/完成事件；保留每总线 worker，并在实板测量两条总线。
- `bsp/artinchip/drv/rtc/drv_rtc.c` 负责 2020-05-20 纪元与 UTC 秒的转换，Framework 继续使用标准时间接口与未对时过滤，不重复实现硬件纪元适配。
- `panel_com.c` 的背光助手只提供固定初始化；现有 RT PWM 端口负责动态亮度及读回。设备、通道等板级事实由 Kconfig/BSP 提供。
- 看门狗仍需验证启动、喂狗、复位和时间预算。诊断快照保护以当前单核、仅线程调用为前提。
- 原生 OTA 自动确认仍不等于应用健康确认。独立 ENV 校验、Flash 读回和激活后重新读取 ENV 保留；断电恢复需要实板证据。

Host 测试和 D13x 镜像构建只能证明源码兼容。板级验收仍需镜像 hash、UART 日志、物理 CAN、显示/背光、RTC、看门狗故障注入及 OTA 断电/重启证据。参见[板级接入](../build/board-integration.zh-CN.md)、[看门狗](watchdog.zh-CN.md)和 [CAN 升级内部实现](../ota/can-update-internals.zh-CN.md)。
