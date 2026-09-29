#ifndef METER_BOARD_SETTINGS_H
#define METER_BOARD_SETTINGS_H
#include "contracts/meter_domain.h"
/** @brief App 在 NVM 加载结束后、所有 CAN worker 启动前锁存速率；运行期不可再调用。 */
bool meter_board_settings_boot(const meter_snapshot_t *snapshot);
/** @brief Protocol 只读启动时锁存的速率，绝不读取 App 的可变快照。 */
uint32_t meter_board_can_bitrate(void);
/** @brief App 独占 PWM；只在亮度变化或失败重试期限到达时执行设备 I/O。 */
bool meter_board_backlight_apply(uint8_t brightness, uint32_t now_ms);
#endif
