#ifndef METER_BOARD_PORT_H
#define METER_BOARD_PORT_H
#include "contracts/meter_can_frame.h"
#include "diagnostics/meter_diagnostics.h"
/* 板级适配：设备名与显示层策略来自 Kconfig（Board resources），不在这里写死某块板。 */
struct rt_semaphore;
/** @brief 读取 SDK 单调硬件毫秒时钟；32 位差值允许正常回绕，不依赖 RTOS tick 初值。 */
uint32_t meter_board_now_ms(void);
/** @brief UI 线程启动显示与触摸（lvgl-aic），按 Kconfig 接管显示层并加载色彩校正。 */
bool meter_board_display_init(void);
/** @brief UI 线程释放显示；仅启动失败路径使用。 */
void meter_board_display_close(meter_diagnostics_t *diag);
/** @brief UI 线程复制显示刷新与触摸驱动的既有统计；无额外采样或串口输出。 */
void meter_board_diagnostics(meter_diagnostics_t *diag);
/** @brief 绑定 RX 唤醒信号量；任一已打开总线收到帧时释放。 */
void meter_board_can_wake(struct rt_semaphore *wake);
/** @brief 运行时线程按 Kconfig 映射打开逻辑总线并设定速率。 */
bool meter_board_can_open(meter_bus_role_t bus, uint32_t bitrate, meter_diagnostics_t *diag);
/** @brief 运行时线程轮询各已打开总线的接收 FIFO，取一帧。 */
bool meter_board_can_read(meter_can_frame_t *frame);
/** @brief 该总线的发送线程调用；驱动可能等待硬件。 */
bool meter_board_can_send(const meter_can_frame_t *frame);
/** @brief 该总线发送 worker 在当前驱动写返回后调用：转入原生只听模式，不发送或 ACK。 */
bool meter_board_can_stop(meter_bus_role_t bus);
/** @brief 复制原生 CAN 驱动的丢包与错误计数。 */
void meter_board_can_diagnostics(meter_diagnostics_t *diag);
#endif
