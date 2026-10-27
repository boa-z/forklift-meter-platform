#ifndef METER_BOARD_SETTINGS_H
#define METER_BOARD_SETTINGS_H
#include "contracts/meter_domain.h"
/** @brief 运行时线程在设置加载结束后、打开 CAN 之前锁存速率；只允许调用一次。 */
bool meter_board_settings_boot(const meter_snapshot_t *snapshot);
/** @brief 启动时锁存的 CAN 速率（bit/s）。 */
uint32_t meter_board_can_bitrate(void);
/** @brief 运行时线程把亮度设置写到背光 PWM；只在变化或失败重试期限到达时执行设备 I/O。 */
bool meter_board_backlight_apply(uint8_t brightness, uint32_t now_ms);
#endif
