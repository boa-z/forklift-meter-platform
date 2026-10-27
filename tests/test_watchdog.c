#include "runtime/meter_watchdog.h"
#include <stdio.h>

#define CHECK(x)                                                                              \
    do                                                                                        \
    {                                                                                         \
        if (!(x))                                                                             \
        {                                                                                     \
            fprintf(stderr, "watchdog:%d: %s\n", __LINE__, #x);                                \
            return 1;                                                                         \
        }                                                                                     \
    } while (0)

#define BIT(ch) (1u << (ch))
#define ALL_OWNERS (BIT(METER_WATCHDOG_APP) | BIT(METER_WATCHDOG_UI))

/* 运行时 100ms、UI 200ms：用于把停滞归因到单一通道。 */
static meter_watchdog_policy_t default_policy(void)
{
    meter_watchdog_policy_t policy = {0};
    policy.enabled = true;
    policy.stall_ms[METER_WATCHDOG_APP] = 100u;
    policy.stall_ms[METER_WATCHDOG_UI] = 200u;
    return policy;
}

static bool step(meter_watchdog_state_t *state, const meter_watchdog_policy_t *policy, uint32_t app, uint32_t ui,
                 uint32_t now_ms)
{
    meter_watchdog_input_t input = {0};
    input.expected = ALL_OWNERS;
    input.counter[METER_WATCHDOG_APP] = app;
    input.counter[METER_WATCHDOG_UI] = ui;
    return meter_watchdog_step(state, policy, &input, now_ms);
}

int main(void)
{
    meter_watchdog_state_t state;
    meter_watchdog_input_t input = {0};

    /* 未启用：即使所有通道停滞也必须喂狗。 */
    meter_watchdog_policy_t policy = default_policy();
    policy.enabled = false;
    meter_watchdog_init(&state);
    input.expected = ALL_OWNERS;
    CHECK(meter_watchdog_step(&state, &policy, &input, 5000u));
    CHECK(!state.supervising && state.feeds == 1u && state.withheld == 0u);

    /* 非法入参不得改变"喂狗"这个保守结论。 */
    CHECK(meter_watchdog_step(NULL, &policy, &input, 0u));
    CHECK(meter_watchdog_step(&state, NULL, &input, 0u));
    CHECK(meter_watchdog_step(&state, &policy, NULL, 0u));
    meter_watchdog_init(NULL);

    policy = default_policy();
    meter_watchdog_init(&state);

    /* expected 为空：运行时启动前落在这里，一律喂狗且不进入监督态。 */
    input = (meter_watchdog_input_t){0};
    CHECK(meter_watchdog_step(&state, &policy, &input, 1000u));
    CHECK(!state.supervising && state.feeds == 1u && state.transitions == 0u);

    /* 首次进入监督态只建立基线：计数尚未变化，不得立即判停滞。 */
    CHECK(step(&state, &policy, 0u, 0u, 1000u));
    CHECK(state.supervising && state.transitions == 1u && state.withheld == 0u);

    /* 判据是"严格超过"：恰好等于上限不算停滞，超过 1ms 即算。 */
    CHECK(step(&state, &policy, 0u, 0u, 1100u));
    CHECK(state.withheld == 0u);
    CHECK(!step(&state, &policy, 0u, 0u, 1101u));
    CHECK(state.stalled == METER_WATCHDOG_APP && state.withheld == 1u && state.stalls == 1u);

    /* 所有通道恢复推进后立即解除停滞，不需要额外复位。 */
    CHECK(step(&state, &policy, 1u, 1u, 1150u));
    CHECK(state.stalled == METER_WATCHDOG_CHANNELS && state.stalls == 1u);

    /* UI 用自己的上限归因：运行时在推进，只有 UI 超限。 */
    CHECK(step(&state, &policy, 2u, 1u, 1300u));
    CHECK(!step(&state, &policy, 3u, 1u, 1351u));
    CHECK(state.stalled == METER_WATCHDOG_UI && state.stalls == 2u);

    /* stall_ms 为 0 的通道只被观察、不参与判停滞。 */
    policy.stall_ms[METER_WATCHDOG_UI] = 0u;
    CHECK(step(&state, &policy, 4u, 1u, 1400u));
    CHECK(state.stalled == METER_WATCHDOG_CHANNELS);
    policy = default_policy();

    /* 通道离开期望集合后必须丢弃基线：重新进入时不得沿用旧时间戳。
       若沿用，UI 的 changed_ms 仍停在 1150，这里会立刻判它停滞。 */
    input = (meter_watchdog_input_t){0};
    input.expected = BIT(METER_WATCHDOG_APP);
    input.counter[METER_WATCHDOG_APP] = 5u;
    CHECK(meter_watchdog_step(&state, &policy, &input, 1450u));
    CHECK(step(&state, &policy, 6u, 9u, 1500u));
    CHECK(step(&state, &policy, 7u, 9u, 1600u));
    CHECK(state.withheld == 2u && state.stalled == METER_WATCHDOG_CHANNELS);

    /* 回到非监督态：清除监督标记，并在下一次进入时重新起算。 */
    const meter_watchdog_input_t idle = {0};
    CHECK(meter_watchdog_step(&state, &policy, &idle, 1700u));
    CHECK(!state.supervising && state.transitions == 2u);

    /* 回绕安全：计数与 now_ms 都按无符号差值比较。 */
    meter_watchdog_init(&state);
    meter_watchdog_input_t wrapped = {0};
    wrapped.expected = BIT(METER_WATCHDOG_APP);
    const uint32_t near_wrap = UINT32_C(0xFFFFFFF0);
    CHECK(meter_watchdog_step(&state, &policy, &wrapped, near_wrap));
    /* 越过 0 点 50ms：未超上限。 */
    CHECK(meter_watchdog_step(&state, &policy, &wrapped, near_wrap + 50u));
    CHECK(state.withheld == 0u);
    /* 越过 0 点 150ms：超上限，必须判停滞。 */
    CHECK(!meter_watchdog_step(&state, &policy, &wrapped, near_wrap + 150u));
    CHECK(state.stalled == METER_WATCHDOG_APP);

    puts("watchdog PASS");
    return 0;
}
