#include "core/meter_core.h"
#include "diagnostics/meter_diagnostics.h"
#include "runtime/meter_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                       \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static const meter_signal_def_t signals[] = {{.id = 7, .key = "vehicle.speed", .unit = "m/s", .stale_ms = 10}};
static const meter_catalog_t catalog = {.signals = signals, .signal_count = 1};
static bool decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    meter_update_t update = {7, {f->data[0], f->timestamp_ms, METER_VALUE_VALID, 1}};
    return sink(ctx, &update);
}
static const meter_protocol_binding_t bindings[] = {{1, decode, NULL}};
static const meter_protocol_profile_t protocols = {bindings, 1};
static const meter_frame_route_t routes[] = {{METER_BUS_CAN0, 123, false, 1}};
static const meter_route_profile_t route_profile = {routes, 1};
static const meter_product_t product = {
    .protocols = &protocols, .routes = &route_profile, .catalog = &catalog};
int main(void)
{
    meter_diagnostics_t d;
    meter_diag_snapshot_t first, second;
    meter_diagnostics_init(&d);
    CHECK(meter_diagnostics_snapshot(&d, 50, &first));
    CHECK(first.uptime_ms == 50 && !first.domain.available && !first.touch.available &&
          first.trace_count == 0);
    meter_core_t core;
    meter_value_t values[1];
    meter_core_storage_t storage = {.signals = values, .signal_capacity = 1};
    CHECK(meter_core_init(&core, &catalog, &storage));
    meter_core_bind_diagnostics(&core, &d);
    meter_runtime_t runtime;
    CHECK(meter_runtime_init(&runtime, &product, meter_core_apply, &core));
    meter_runtime_bind_diagnostics(&runtime, &d);
    meter_diagnostics_time(&d, 100);
    meter_runtime_connection(&runtime, true);
    meter_core_connection(&core, true, runtime.generation);
    CHECK(core.snapshot.revision == 1);
    meter_can_frame_t f = {.bus = METER_BUS_CAN0, .id = 123, .size = 1, .timestamp_ms = 100, .data = {42}};
    CHECK(meter_runtime_push(&runtime, &f));
    CHECK(meter_runtime_poll(&runtime, 1) == 1);
    CHECK(meter_diagnostics_snapshot(&d, 101, &first));
    CHECK(first.runtime.counters.accepted == 1 && first.runtime.counters.dispatched == 1 &&
          first.runtime.depth == 0);
    CHECK(first.runtime.counters.resets == 1 && first.can[0].rx == 1);
    CHECK(first.domain.updates == 1 && first.domain.valid_transition == 1 && first.domain.signals == 1);
    meter_diag_signal_t signal;
    CHECK(meter_diagnostics_signal(&d, "vehicle.speed", 101, &signal));
    CHECK(signal.value.value == 42 && signal.age_ms == 1 && signal.stale_ms == 10 &&
          strcmp(signal.unit, "m/s") == 0);
    CHECK(!meter_diagnostics_signal(&d, "missing", 101, &signal));
    uint32_t revision = core.snapshot.revision;
    CHECK(meter_runtime_push(&runtime, &f));
    meter_runtime_poll(&runtime, 1);
    CHECK(core.snapshot.revision == revision);
    meter_core_tick(&core, 110);
    meter_core_tick(&core, 111);
    CHECK(d.data.domain.stale_transition == 1);
    meter_update_t update = {7, {43, 112, METER_VALUE_VALID, 2}};
    CHECK(meter_core_apply(&core, &update));
    update.value.state = METER_VALUE_ERROR;
    CHECK(meter_core_apply(&core, &update));
    CHECK(meter_diagnostics_snapshot(&d, 113, &second));
    CHECK(second.domain.updates == 4 && second.domain.valid_transition == 2 &&
          second.domain.source_switch == 1 && second.domain.error_updates == 1);
    CHECK(first.domain.updates == 1 && first.domain.stale_transition == 0);
    for (unsigned i = 0; i < METER_RX_CAPACITY; ++i)
        CHECK(meter_runtime_push(&runtime, &f));
    CHECK(!meter_runtime_push(&runtime, &f));
    CHECK(d.data.can[0].rx_drop == 1);
    CHECK(d.data.runtime.counters.overflow == 1 && d.data.runtime.depth == METER_RX_CAPACITY);
    meter_runtime_connection(&runtime, false);
    CHECK(d.data.runtime.depth == 0 && d.data.runtime.counters.resets == 2);
    d.data.ui.present_count = UINT32_MAX;
    METER_DIAG_INC(&d, ui, present_count);
    CHECK(d.data.ui.present_count == UINT32_MAX);
    values[0].timestamp_ms = UINT32_MAX - 4;
    CHECK(meter_diagnostics_signal(&d, "vehicle.speed", 3, &signal) && signal.age_ms == 8);
    CHECK(!meter_diagnostics_snapshot(NULL, 0, &first));
    return 0;
}
