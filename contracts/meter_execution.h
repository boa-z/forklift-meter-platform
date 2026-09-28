#ifndef METER_EXECUTION_H
#define METER_EXECUTION_H
#include <stdbool.h>
#include <stdint.h>

/** @brief App 独占生命周期；STOPPING 超时保留资源，不能伪造 STOPPED。 */
typedef enum
{
    METER_EXEC_INIT, METER_EXEC_READY, METER_EXEC_RUNNING,
    METER_EXEC_STOPPING, METER_EXEC_STOPPED, METER_EXEC_FAILED
} meter_execution_state_t;
/** @brief App 独占模式；其他 owner 只消费复制的策略和 generation。 */
typedef enum
{
    METER_MODE_STARTUP, METER_MODE_NORMAL, METER_MODE_DEGRADED,
    METER_MODE_UPDATE_MAINTENANCE, METER_MODE_SHUTDOWN
} meter_mode_t;
/** @brief Product 为每个模式声明允许的能力；不包含设备或跨线程对象指针。 */
typedef struct
{
    bool telemetry, commands, critical_tx, ordinary_tx, settings, normal_ui;
} meter_mode_policy_t;
/** @brief 只允许 Protocol 调用的周期调度配置；毫秒间隔必须小于半周期。 */
typedef struct
{
    uint32_t period_ms, next_ms, missed, late_ms;
    bool armed;
} meter_deadline_t;
#endif
