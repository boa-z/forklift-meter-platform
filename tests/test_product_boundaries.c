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

static void test_independent_twenty_and_fifty_ms_periods(void)
{
    const meter_periodic_frame_t fast = {.frame = {.bus = METER_BUS_CAN0, .id = 0x123u}, .period_ms = 20u};
    const meter_periodic_frame_t slow = {.frame = {.bus = METER_BUS_CAN1, .id = 0x123u}, .period_ms = 50u};
    meter_periodic_state_t fast_state, slow_state;
    meter_periodic_message_t message;
    assert(meter_periodic_reset(&fast_state, &fast, 1u, 0u));
    assert(meter_periodic_reset(&slow_state, &slow, 1u, 0u));
    assert(!meter_periodic_prepare(&fast_state, &fast, NULL, true, 19u, &message));
    assert(meter_periodic_prepare(&fast_state, &fast, NULL, true, 20u, &message));
    assert(message.frame.bus == METER_BUS_CAN0 && message.deadline_ms == 20u);
    assert(!meter_periodic_prepare(&slow_state, &slow, NULL, true, 49u, &message));
    assert(meter_periodic_prepare(&slow_state, &slow, NULL, true, 50u, &message));
    assert(message.frame.bus == METER_BUS_CAN1 && message.deadline_ms == 50u);
    assert(meter_periodic_prepare(&fast_state, &fast, NULL, true, 100u, &message));
    assert(message.deadline_ms == 100u && fast_state.deadline.missed == 3u);
    assert(!meter_periodic_prepare(&fast_state, &fast, NULL, true, 100u, &message));
    assert(meter_periodic_prepare(&slow_state, &slow, NULL, true, 100u, &message));
    assert(message.deadline_ms == 100u && slow_state.deadline.missed == 0u);
    assert(!meter_periodic_prepare(&slow_state, &slow, NULL, true, 100u, &message));
}

int main(void)
{
    test_overlapping_bus_and_format_identity();
    test_timeout_keeps_source_value_and_recovers();
    test_independent_twenty_and_fifty_ms_periods();
    return 0;
}
