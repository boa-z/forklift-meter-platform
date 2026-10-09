#ifndef METER_BOARD_PORT_H
#define METER_BOARD_PORT_H
#include "platform/rtthread/meter_rtthread_adapter.h"
/** @brief 构造 reference-board Demo 板端接口；diag 在设备整个生命期有效，所有函数由 owner 线程调用。 */
meter_rtthread_board_port_t meter_board_port(meter_diagnostics_t *diag);
/** @brief owner 在 LVGL tick 后复制驱动已有统计；无额外采样或串口输出。 */
void meter_board_diagnostics(meter_diagnostics_t *diag);
/** @brief 初始化失败时关闭本应用已打开的设备；owner 锁内调用。 */
void meter_board_close(meter_diagnostics_t *diag);
/** @brief 读取 SDK 单调硬件毫秒时钟；32 位差值允许正常回绕，不依赖 RTOS tick 初值。 */
uint32_t meter_board_now_ms(void);
struct rt_semaphore;
/** @brief 初始化时绑定原生 RX 唤醒信号量；设备关闭前不得释放。 */
void meter_board_can_wake(struct rt_semaphore *wake);
/** @brief Protocol 与 TX 全部停止后关闭 CAN；不操作 LVGL。 */
void meter_board_can_close(meter_diagnostics_t *diag);
/** @brief Protocol owner 独占读取硬件接收 FIFO。 */
bool meter_board_can_raw_read(void *, meter_can_frame_t *);
/** @brief 独立 TX worker 调用可能等待硬件的原生发送接口。 */
bool meter_board_can_send(const meter_can_frame_t *);
/** @brief Protocol 复制原生 CAN 驱动计数，保留硬件 FIFO 丢包证据。 */
void meter_board_can_diagnostics(meter_diagnostics_t *diag);
#endif
