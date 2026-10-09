#include "runtime/meter_watchdog.h"
#include "contracts/meter_time.h"

/* 通道停滞判据用无符号差值与半个周期比较，因此回绕安全；
   这与 runtime/meter_periodic.c 的期限判据同源。 */
static bool elapsed_past(uint32_t now_ms, uint32_t since_ms, uint32_t limit_ms)
{
    return (now_ms - since_ms) > limit_ms && (now_ms - since_ms) < METER_TIME_HALF_RANGE;
}

void meter_watchdog_init(meter_watchdog_state_t *state)
{
    if (!state)
        return;
    *state = (meter_watchdog_state_t){0};
    state->stalled = METER_WATCHDOG_CHANNELS;
}

bool meter_watchdog_step(meter_watchdog_state_t *state, const meter_watchdog_policy_t *policy,
                        const meter_watchdog_input_t *input, uint32_t now_ms)
{
    if (!state || !policy || !input)
        return true;
    if (!policy->enabled || input->expected == 0u)
    {
        /* 非监督态：启动前与停止流程都落在这里，一律喂狗，且丢弃旧基线，
           使下一次进入监督态时重新获得完整的 stall 窗口。 */
        if (state->supervising)
        {
            ++state->transitions;
            for (unsigned i = 0u; i < METER_WATCHDOG_CHANNELS; ++i)
                state->seen[i] = false;
        }
        state->supervising = false;
        state->stalled = METER_WATCHDOG_CHANNELS;
        ++state->feeds;
        return true;
    }
    if (!state->supervising)
    {
        /* 刚进入监督态：只建立基线，不立即判停滞。 */
        ++state->transitions;
        state->supervising = true;
        for (unsigned i = 0u; i < METER_WATCHDOG_CHANNELS; ++i)
            state->seen[i] = false;
    }
    meter_watchdog_channel_t stalled = METER_WATCHDOG_CHANNELS;
    for (unsigned i = 0u; i < METER_WATCHDOG_CHANNELS; ++i)
    {
        const uint32_t bit = 1u << i;
        if (!(input->expected & bit))
        {
            /* 不再期望的通道要清掉基线：它下次进入期望集合时必须重新起算。 */
            state->seen[i] = false;
            continue;
        }
        if (!state->seen[i] || state->observed[i] != input->counter[i])
        {
            state->seen[i] = true;
            state->observed[i] = input->counter[i];
            state->changed_ms[i] = now_ms;
            continue;
        }
        /* limit 为 0 的通道只被观察，不参与判停滞。 */
        if (policy->stall_ms[i] != 0u && elapsed_past(now_ms, state->changed_ms[i], policy->stall_ms[i]) &&
            stalled == METER_WATCHDOG_CHANNELS)
            stalled = (meter_watchdog_channel_t)i;
    }
    if (stalled == METER_WATCHDOG_CHANNELS)
    {
        state->stalled = METER_WATCHDOG_CHANNELS;
        ++state->feeds;
        return true;
    }
    /* 第一个停滞通道决定结论；其余通道继续采样，恢复推进后自动解除。 */
    state->stalled = stalled;
    ++state->withheld;
    ++state->stalls;
    return false;
}
