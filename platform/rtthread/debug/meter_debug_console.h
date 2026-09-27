#ifndef METER_DEBUG_CONSOLE_H
#define METER_DEBUG_CONSOLE_H
#include "diagnostics/meter_diagnostics.h"
/** @brief 初始化静态 RT-Thread mutex，绑定运行期有效的身份和诊断实例；仅启动线程调用一次。 */
bool meter_debug_init(meter_diagnostics_t *diag, const meter_build_info_t *build);
/** @brief owner 更新数据前取原生 mutex；线程上下文，禁止 ISR 调用。 */
void meter_debug_lock(void);
/** @brief owner 更新结束后释放 mutex；不得在锁内执行串口查询输出。 */
void meter_debug_unlock(void);
/** @brief 从诊断历史取增量事件映射到原生 ULog；owner 解锁后调用，无自研输出队列。 */
void meter_debug_log_drain(void);
/** @brief 将 LVGL 日志回调接入 ULog；LVGL 初始化后、创建对象前调用。 */
void meter_debug_lvgl_log_init(void);
#endif
