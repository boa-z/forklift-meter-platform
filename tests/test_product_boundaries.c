#include "core/meter_core.h"
#include "protocols/common/meter_frame_router.h"
#include "runtime/meter_periodic.h"
#include <assert.h>
#include <math.h>

static void test_overlapping_bus_and_format_identity(void)
{
    meter_frame_route_t entries[] = {{METER_BUS_CAN0, 0x123u, false, 1u},
                                     {METER_BUS_CAN1, 0x123u, false, 2u},
                                     {METER_BUS_CAN0, 0x123u, true, 3u}};
    const meter_route_profile_t routes = {entries, 3u};
    assert(meter_routes_valid(&routes));
    meter_can_frame_t frame = {.bus = METER_BUS_CAN0, .id = 0x123u, .size = 1u};
    assert(meter_route_lookup(&routes, &frame)->owner == 1u);
    frame.bus = METER_BUS_CAN1;
    assert(meter_route_lookup(&routes, &frame)->owner == 2u);
    frame.extended = true;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    frame.bus = METER_BUS_CAN0;
    assert(meter_route_lookup(&routes, &frame)->owner == 3u);
    frame.remote = true;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    entries[1].bus = METER_BUS_CAN0;
    assert(!meter_routes_valid(&routes));
}

static void test_fallback_is_scoped_and_exact_routes_win(void)
{
    meter_frame_route_t entries[] = {{METER_BUS_CAN0, METER_CAN_ID_ANY, false, 1u},
                                    {METER_BUS_CAN0, 0x123u, false, 2u},
                                    {METER_BUS_CAN1, METER_CAN_ID_ANY, true, 3u}};
    const meter_route_profile_t routes = {entries, 3u};
    assert(meter_routes_valid(&routes));
    meter_can_frame_t frame = {.bus = METER_BUS_CAN0, .id = 0x123u, .size = 8u};
    assert(meter_route_lookup(&routes, &frame)->owner == 2u);
    frame.id = 0x456u;
    assert(meter_route_lookup(&routes, &frame)->owner == 1u);
    frame.extended = true;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    frame.bus = METER_BUS_CAN1;
    assert(meter_route_lookup(&routes, &frame)->owner == 3u);
    frame.extended = false;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    frame.bus = METER_BUS_CAN0;
    frame.id = 0x800u;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    frame.id = 0x456u;
    frame.size = 9u;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    frame.size = 8u;
    frame.remote = true;
    assert(meter_route_lookup(&routes, &frame) == NULL);
    entries[1].id = METER_CAN_ID_ANY;
    assert(!meter_routes_valid(&routes));
}

static void test_timeout_keeps_source_value_and_recovers(void)
{
    const meter_signal_def_t signals[] = {{.id = 1u, .stale_ms = 20u}, {.id = 2u, .stale_ms = 50u}};
    const meter_catalog_t catalog = {.signals = signals, .signal_count = 2u};
    meter_value_t values[2];
    const meter_core_storage_t storage = {.signals = values, .signal_capacity = 2u};
    meter_core_t core;
    assert(meter_core_init(&core, &catalog, &storage));
    assert(values[0].state == METER_VALUE_UNKNOWN && values[0].source == METER_SOURCE_NONE);
    const uint32_t start = UINT32_MAX - 10u;
    meter_update_t first = {1u, {42.0f, start, METER_VALUE_VALID, 11u}};
    meter_update_t second = {2u, {17.0f, start, METER_VALUE_VALID, 12u}};
    assert(meter_core_apply(&core, &first));
    assert(meter_core_apply(&core, &second));
    meter_core_tick(&core, start + 19u);
    assert(values[0].state == METER_VALUE_VALID);
    meter_core_tick(&core, start + 20u);
    assert(values[0].state == METER_VALUE_STALE && values[1].state == METER_VALUE_VALID);
    assert(values[0].value == 42.0f && values[0].timestamp_ms == start && values[0].source == 11u);
    meter_core_tick(&core, start + 50u);
    assert(values[1].state == METER_VALUE_STALE && values[1].value == 17.0f && values[1].source == 12u);
    first.value.value = 43.0f;
    first.value.timestamp_ms = start + 51u;
    assert(meter_core_apply(&core, &first));
    assert(values[0].state == METER_VALUE_VALID && values[0].value == 43.0f && values[0].source == 11u);
    meter_core_connection(&core, false, 2u);
    assert(values[0].state == METER_VALUE_STALE && values[0].value == 43.0f);
    meter_core_connection(&core, true, 3u);
    assert(values[0].state == METER_VALUE_STALE); /* Connection alone is not new data. */
    first.value.value = NAN;
    assert(meter_core_apply(&core, &first));
    assert(values[0].state == METER_VALUE_ERROR); /* Never a valid zero; no separate last-good history. */
}

/* 帧在运行时一轮里按读取时刻打戳，比这一轮开头取的 now 更晚几毫秒：这样的值不是过期，更不能因无符号回绕被判成过期。 */
static void test_timestamp_slightly_ahead_of_tick_is_not_stale(void)
{
    const meter_signal_def_t signals[] = {{.id = 1u, .stale_ms = 500u}};
    const meter_catalog_t catalog = {.signals = signals, .signal_count = 1u};
    meter_value_t values[1];
    const meter_core_storage_t storage = {.signals = values, .signal_capacity = 1u};
    meter_core_t core;
    assert(meter_core_init(&core, &catalog, &storage));
    for (uint32_t now = UINT32_MAX - 3u; now != 6u; ++now) /* 含无符号回绕 */
    {
        meter_update_t update = {1u, {1.0f, now + 2u, METER_VALUE_VALID, 11u}};
        assert(meter_core_apply(&core, &update));
        meter_core_tick(&core, now);
        assert(values[0].state == METER_VALUE_VALID);
    }
    meter_update_t update = {1u, {1.0f, 1000u, METER_VALUE_VALID, 11u}};
    assert(meter_core_apply(&core, &update));
    meter_core_tick(&core, 1499u);
    assert(values[0].state == METER_VALUE_VALID);
    meter_core_tick(&core, 1500u); /* 期限不变：恰好 stale_ms 之后过期 */
    assert(values[0].state == METER_VALUE_STALE);
}

static void test_independent_twenty_and_fifty_ms_periods(void)
{
    const meter_periodic_frame_t fast = {.frame = {.bus = METER_BUS_CAN0, .id = 0x123u}, .period_ms = 20u};
    const meter_periodic_frame_t slow = {.frame = {.bus = METER_BUS_CAN1, .id = 0x123u}, .period_ms = 50u};
    meter_deadline_t fast_state = {0}, slow_state = {0};
    meter_can_frame_t frame;
    /* 第一帧在启用时立即发出，之后两个条目各按自己的周期。 */
    assert(meter_periodic_due(&fast_state, &fast, NULL, 0u, true, false, &frame) && frame.bus == METER_BUS_CAN0);
    assert(meter_periodic_due(&slow_state, &slow, NULL, 0u, true, false, &frame) && frame.bus == METER_BUS_CAN1);
    assert(!meter_periodic_due(&fast_state, &fast, NULL, 19u, true, false, &frame));
    assert(meter_periodic_due(&fast_state, &fast, NULL, 20u, true, false, &frame) && frame.timestamp_ms == 20u);
    assert(!meter_periodic_due(&slow_state, &slow, NULL, 49u, true, false, &frame));
    assert(meter_periodic_due(&slow_state, &slow, NULL, 50u, true, false, &frame));
    /* 迟到只补一帧并记录跳过的周期，不补发突发。 */
    assert(meter_periodic_due(&fast_state, &fast, NULL, 100u, true, false, &frame));
    assert(fast_state.missed == 3u && fast_state.next_ms == 120u);
    assert(!meter_periodic_due(&fast_state, &fast, NULL, 100u, true, false, &frame));
    assert(meter_periodic_due(&slow_state, &slow, NULL, 100u, true, false, &frame) && slow_state.missed == 0u);
    /* 不允许发送或上一帧仍在途：期限照常推进，不生成帧。 */
    assert(!meter_periodic_due(&slow_state, &slow, NULL, 150u, false, false, &frame) && slow_state.next_ms == 200u);
    assert(!meter_periodic_due(&slow_state, &slow, NULL, 200u, true, true, &frame) && slow_state.next_ms == 250u);
}

static bool encode_level(const meter_snapshot_t *snapshot, uint32_t now, meter_can_frame_t *frame)
{
    (void)now;
    const meter_value_t level = meter_snapshot_read(snapshot, 1u);
    /* 值不可用时本周期不发；可用时把值写进模板的第二个字节。 */
    if (level.state != METER_VALUE_VALID)
        return false;
    frame->data[1] = (uint8_t)level.value;
    return true;
}

static void test_encoder_reads_the_snapshot(void)
{
    static const meter_signal_def_t signals[] = {{"level", "%", 0u, 1u}};
    static const meter_catalog_t catalog = {.signals = signals, .signal_count = 1u};
    meter_value_t values[1];
    meter_core_storage_t storage = {.signals = values, .signal_capacity = 1u};
    meter_core_t core;
    assert(meter_core_init(&core, &catalog, &storage));
    const meter_periodic_frame_t definition = {
        .frame = {.bus = METER_BUS_CAN0, .id = 0x3c0u, .size = 2u, .data = {0xA5u}}, .period_ms = 10u,
        .encode = encode_level};
    assert(meter_periodic_valid(&definition));
    meter_deadline_t deadline = {0};
    meter_can_frame_t frame;
    assert(!meter_periodic_due(&deadline, &definition, &core.snapshot, 0u, true, false, &frame));
    const meter_update_t level = {1u, {37.0f, 5u, METER_VALUE_VALID, 1u}};
    assert(meter_core_apply(&core, &level));
    assert(meter_periodic_due(&deadline, &definition, &core.snapshot, 10u, true, false, &frame));
    assert(frame.id == 0x3c0u && frame.size == 2u && frame.data[0] == 0xA5u && frame.data[1] == 37u);
    const meter_periodic_frame_t broken = {.frame = {.bus = METER_BUS_COUNT}, .period_ms = 10u};
    assert(!meter_periodic_valid(&broken));
}

int main(void)
{
    test_encoder_reads_the_snapshot();
    test_overlapping_bus_and_format_identity();
    test_fallback_is_scoped_and_exact_routes_win();
    test_timeout_keeps_source_value_and_recovers();
    test_timestamp_slightly_ahead_of_tick_is_not_stale();
    test_independent_twenty_and_fifty_ms_periods();
    return 0;
}
