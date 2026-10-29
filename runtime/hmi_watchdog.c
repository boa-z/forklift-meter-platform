#include "runtime/hmi_watchdog.h"
#include "contracts/hmi_time.h"

/* 通道停滞判据用无符号差值与半个周期比较，因此回绕安全；
   这与 runtime/hmi_periodic.c 的期限判据同源。 */
static bool elapsed_past(uint32_t now_ms, uint32_t since_ms, uint32_t limit_ms)
{
    return (now_ms - since_ms) > limit_ms && (now_ms - since_ms) < HMI_TIME_HALF_RANGE;
}

void hmi_watchdog_init(hmi_watchdog_state_t *state)
{
    if (!state)
        return;
    *state = (hmi_watchdog_state_t){0};
    state->stalled = HMI_WATCHDOG_CHANNELS;
}

bool hmi_watchdog_step(hmi_watchdog_state_t *state, const hmi_watchdog_policy_t *policy,
                        const hmi_watchdog_input_t *input, uint32_t now_ms)
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
            for (unsigned i = 0u; i < HMI_WATCHDOG_CHANNELS; ++i)
                state->seen[i] = false;
        }
        state->supervising = false;
        state->stalled = HMI_WATCHDOG_CHANNELS;
        ++state->feeds;
        return true;
    }
    if (!state->supervising)
    {
        /* 刚进入监督态：只建立基线，不立即判停滞。 */
        ++state->transitions;
        state->supervising = true;
        for (unsigned i = 0u; i < HMI_WATCHDOG_CHANNELS; ++i)
            state->seen[i] = false;
    }
    hmi_watchdog_channel_t stalled = HMI_WATCHDOG_CHANNELS;
    for (unsigned i = 0u; i < HMI_WATCHDOG_CHANNELS; ++i)
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
            stalled == HMI_WATCHDOG_CHANNELS)
            stalled = (hmi_watchdog_channel_t)i;
    }
    if (stalled == HMI_WATCHDOG_CHANNELS)
    {
        state->stalled = HMI_WATCHDOG_CHANNELS;
        ++state->feeds;
        return true;
    }
    /* 第一个停滞通道决定结论；其余通道继续采样，恢复推进后自动解除。 */
    state->stalled = stalled;
    ++state->withheld;
    ++state->stalls;
    return false;
}
