#ifndef METER_WATCHDOG_H
#define METER_WATCHDOG_H
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 监督通道：每个应当持续推进的常驻 owner 一个心跳计数。
 *
 * 本模块只做判据，不访问设备、不创建线程，因此可以在主机上完整测试。
 * 启动阶段（runtime 尚未启动、显示与存储还在初始化）不在监督范围内，
 * 由平台端口选择何时进入监督态，见 docs/runtime/watchdog.md。
 */
typedef enum
{
    METER_WATCHDOG_PROTOCOL = 0,
    METER_WATCHDOG_APP,
    METER_WATCHDOG_UI,
    METER_WATCHDOG_CHANNELS
} meter_watchdog_channel_t;

/** @brief 一次采样的计数与期望掩码；expected 的位按 meter_watchdog_channel_t 编号。 */
typedef struct
{
    uint32_t counter[METER_WATCHDOG_CHANNELS];
    uint32_t expected;
} meter_watchdog_input_t;

/** @brief 每个通道允许的最长停滞时间；0 表示该通道不参与判据。 */
typedef struct
{
    uint32_t stall_ms[METER_WATCHDOG_CHANNELS];
    bool enabled;
} meter_watchdog_policy_t;

/** @brief 监督状态；诊断只读取其副本，不参与判据。 */
typedef struct
{
    uint32_t observed[METER_WATCHDOG_CHANNELS];
    uint32_t changed_ms[METER_WATCHDOG_CHANNELS];
    bool seen[METER_WATCHDOG_CHANNELS];
    bool supervising;
    meter_watchdog_channel_t stalled;
    uint32_t feeds, withheld, stalls, transitions;
} meter_watchdog_state_t;

/** @brief 把状态复位到"尚未观察任何通道"；policy 与 input 都不保留。 */
void meter_watchdog_init(meter_watchdog_state_t *state);

/**
 * @brief 纯判据：返回 true 表示应当喂硬件看门狗。
 *
 * expected 为 0 时进入非监督态并一律返回 true：启动阶段与停止流程都不得复位。
 * 通道首次出现在 expected 中时只建立基线，不会立即判为停滞；此后计数在
 * stall_ms 内没有变化即判停滞，且只在期望集合内选第一个停滞通道。
 * 计数按无符号回绕比较，now_ms 也允许回绕。
 */
bool meter_watchdog_step(meter_watchdog_state_t *state, const meter_watchdog_policy_t *policy,
                        const meter_watchdog_input_t *input, uint32_t now_ms);
#endif
