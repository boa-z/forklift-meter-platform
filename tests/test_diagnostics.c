#include "core/hmi_core.h"
#include "diagnostics/hmi_diagnostics.h"
#include "runtime/hmi_runtime.h"
#include <stdio.h>
#include <string.h>
#include "check.h"
static const hmi_signal_def_t signals[] = {{.id = 7, .key = "vehicle.speed", .unit = "m/s", .stale_ms = 10}};
static const hmi_catalog_t catalog = {.signals = signals, .signal_count = 1};
static bool decode(const hmi_can_frame_t *f, hmi_update_sink_t sink, void *ctx)
{
    hmi_update_t update = {7, {f->data[0], f->timestamp_ms, HMI_VALUE_VALID, 1}};
    return sink(ctx, &update);
}
static const hmi_protocol_binding_t bindings[] = {{1, decode, NULL}};
static const hmi_protocol_profile_t protocols = {bindings, 1};
static const hmi_frame_route_t routes[] = {{HMI_BUS_CAN0, 123, false, 1}};
static const hmi_route_profile_t route_profile = {routes, 1};
static const hmi_product_t product = {
    .protocols = &protocols, .routes = &route_profile, .catalog = &catalog};
int main(void)
{
    hmi_diagnostics_t d;
    hmi_diag_snapshot_t first, second;
    hmi_diagnostics_init(&d);
    CHECK(hmi_diagnostics_snapshot(&d, 50, &first));
    CHECK(first.uptime_ms == 50 && !first.domain.available && !first.touch.available &&
          first.trace_count == 0);
    hmi_core_t core;
    hmi_value_t values[1];
    hmi_core_storage_t storage = {.signals = values, .signal_capacity = 1};
    CHECK(hmi_core_init(&core, &catalog, &storage));
    hmi_core_bind_diagnostics(&core, &d);
    hmi_runtime_t runtime;
    CHECK(hmi_runtime_init(&runtime, &product, hmi_core_apply, &core));
    hmi_runtime_bind_diagnostics(&runtime, &d);
    hmi_diagnostics_time(&d, 100);
    hmi_runtime_connection(&runtime, true);
    hmi_core_connection(&core, true, runtime.generation);
    CHECK(core.snapshot.revision == 1);
    hmi_can_frame_t f = {.bus = HMI_BUS_CAN0, .id = 123, .size = 1, .timestamp_ms = 100, .data = {42}};
    CHECK(hmi_runtime_receive(&runtime, &f));
    CHECK(hmi_diagnostics_snapshot(&d, 101, &first));
    CHECK(first.runtime.counters.accepted == 1 && first.runtime.counters.dispatched == 1 &&
          first.runtime.adapters == 1);
    CHECK(first.runtime.counters.resets == 1 && first.can[0].rx == 1);
    CHECK(first.domain.updates == 1 && first.domain.valid_transition == 1 && first.domain.signals == 1);
    hmi_diag_signal_t signal;
    CHECK(hmi_diagnostics_signal(&d, "vehicle.speed", 101, &signal));
    CHECK(signal.value.value == 42 && signal.age_ms == 1 && signal.stale_ms == 10 &&
          strcmp(signal.unit, "m/s") == 0);
    CHECK(!hmi_diagnostics_signal(&d, "missing", 101, &signal));
    uint32_t revision = core.snapshot.revision;
    CHECK(hmi_runtime_receive(&runtime, &f));
    CHECK(core.snapshot.revision == revision);
    hmi_core_tick(&core, 110);
    hmi_core_tick(&core, 111);
    CHECK(d.data.domain.stale_transition == 1);
    hmi_update_t update = {7, {43, 112, HMI_VALUE_VALID, 2}};
    CHECK(hmi_core_apply(&core, &update));
    update.value.state = HMI_VALUE_ERROR;
    CHECK(hmi_core_apply(&core, &update));
    CHECK(hmi_diagnostics_snapshot(&d, 113, &second));
    CHECK(second.domain.updates == 4 && second.domain.valid_transition == 2 &&
          second.domain.source_switch == 1 && second.domain.error_updates == 1);
    CHECK(first.domain.updates == 1 && first.domain.stale_transition == 0);
    /* 断开后收到的帧只计 rx_drop，不进入解码。 */
    hmi_runtime_connection(&runtime, false);
    CHECK(!hmi_runtime_receive(&runtime, &f));
    CHECK(d.data.can[0].rx_drop == 1 && d.data.runtime.counters.resets == 2 && !d.data.runtime.connected);
    d.data.ui.present_count = UINT32_MAX;
    HMI_DIAG_INC(&d, ui, present_count);
    CHECK(d.data.ui.present_count == UINT32_MAX);
    values[0].timestamp_ms = UINT32_MAX - 4;
    CHECK(hmi_diagnostics_signal(&d, "vehicle.speed", 3, &signal) && signal.age_ms == 8);
    CHECK(!hmi_diagnostics_snapshot(NULL, 0, &first));
    return 0;
}
