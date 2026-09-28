#include "runtime/meter_execution.h"
#include "runtime/meter_runtime.h"
#include "core/meter_core.h"
#include <assert.h>
#include <string.h>
static const meter_signal_def_t definitions[] = {{.id = 1u, .key = "a", .unit = "", .stale_ms = 0u}, {.id = 2u, .key = "b", .unit = "", .stale_ms = 10u}};
static bool reject_second;
static bool arbitration(void *ctx, meter_signal_id_t id, const meter_value_t *incoming, const meter_value_t *current)
{
    (void)ctx; (void)incoming; (void)current; return !(reject_second && id == 2u);
}
static const meter_catalog_t catalog = {.signals = definitions, .signal_count = 2u, .source_policy = arbitration};
static bool decode_ok = true;
static bool decode(const meter_can_frame_t *frame, meter_update_sink_t sink, void *ctx)
{
    meter_update_t first = {1u, {1.0f, frame->timestamp_ms, METER_VALUE_VALID, 1u}};
    meter_update_t second = {2u, {2.0f, frame->timestamp_ms, METER_VALUE_VALID, 1u}};
    bool ok = sink(ctx, &first);
    ok = sink(ctx, &second) && ok;
    return decode_ok && ok;
}
static unsigned submitted;
static bool full;
static meter_batch_result_t submit(void *ctx, const meter_update_batch_t *batch)
{
    (void)ctx; assert(batch->count == 2u); ++submitted;
    return full ? METER_BATCH_IPC_FULL : METER_BATCH_QUEUED;
}
int main(void)
{
    meter_execution_t state; meter_execution_init(&state);
    assert(!meter_execution_transition(&state, METER_EXEC_RUNNING));
    assert(!meter_execution_mode(&state, METER_MODE_NORMAL));
    assert(meter_execution_transition(&state, METER_EXEC_READY));
    assert(meter_execution_transition(&state, METER_EXEC_RUNNING));
    assert(meter_execution_mode(&state, METER_MODE_NORMAL));
    assert(state.generation == 2u);
    assert(meter_execution_mode(&state, METER_MODE_NORMAL) && state.generation == 2u);
    assert(meter_execution_mode(&state, METER_MODE_UPDATE_MAINTENANCE));
    assert(!meter_execution_policy(state.mode).settings);
    assert(meter_execution_policy(state.mode).critical_tx);
    assert(!meter_execution_mode(&state, (meter_mode_t)99));
    assert(meter_execution_transition(&state, METER_EXEC_STOPPING));
    assert(!meter_execution_transition(&state, METER_EXEC_RUNNING));
    assert(meter_execution_transition(&state, METER_EXEC_STOPPED));
    meter_deadline_t deadline;
    assert(!meter_deadline_arm(&deadline, 0u, 0u));
    assert(meter_deadline_arm(&deadline, 0xfffffff0u, 50u));
    assert(!meter_deadline_take(&deadline, 20u));
    assert(meter_deadline_take(&deadline, 35u) && deadline.next_ms == 84u);
    assert(meter_deadline_take(&deadline, 240u) && deadline.next_ms == 284u && deadline.missed == 3u);
    assert(!meter_deadline_take(&deadline, 240u));
    meter_core_t core; meter_value_t values[2];
    meter_core_storage_t store = {.signals = values, .signal_capacity = 2u};
    assert(meter_core_init(&core, &catalog, &store));
    meter_core_connection(&core, true, 2u);
    meter_update_t updates[] = {{1u, {11.0f, 2u, METER_VALUE_VALID, 1u}}, {2u, {22.0f, 2u, METER_VALUE_VALID, 1u}}};
    meter_update_batch_t batch = {.generation = 2u, .sequence = 1u, .source = 1u, .updates = updates, .count = 2u};
    uint32_t revision = core.snapshot.revision;
    reject_second = true;
    assert(!meter_core_apply_batch(&core, &batch));
    assert(core.snapshot.revision == revision && values[0].state == METER_VALUE_UNKNOWN);
    reject_second = false;
    updates[1].signal = 99u; assert(!meter_core_apply_batch(&core, &batch));
    assert(core.snapshot.revision == revision);
    updates[1].signal = 2u; batch.generation = 1u; assert(!meter_core_apply_batch(&core, &batch));
    batch.generation = 2u; assert(meter_core_apply_batch(&core, &batch));
    assert(values[0].value == 11.0f && values[1].value == 22.0f);
    revision = core.snapshot.revision; assert(meter_core_apply_batch(&core, &batch));
    assert(core.snapshot.revision == revision);
    static const meter_frame_route_t route[] = {{METER_BUS_CAN0, 0x100u, false, 1u}};
    static const meter_route_profile_t routes = {route, 1u};
    static const meter_protocol_binding_t binding[] = {{.owner = 1u, .decode = decode}};
    static const meter_protocol_profile_t profile = {binding, 1u};
    static const meter_product_t product = {.routes = &routes, .protocols = &profile};
    meter_runtime_t runtime; meter_batch_builder_t builder; meter_update_t staged[2];
    assert(meter_batch_builder_init(&builder, staged, 2u));
    assert(meter_runtime_init(&runtime, &product, meter_batch_builder_add, &builder));
    assert(meter_runtime_bind_batches(&runtime, &builder, submit, NULL));
    meter_runtime_connection(&runtime, true);
    meter_can_frame_t frame = {.bus = METER_BUS_CAN0, .id = 0x100u, .size = 8u};
    assert(meter_runtime_push(&runtime, &frame)); assert(meter_runtime_poll(&runtime, 1u) == 1u);
    assert(submitted == 1u && runtime.diagnostics.dispatched == 1u);
    decode_ok = false; assert(meter_runtime_push(&runtime, &frame)); (void)meter_runtime_poll(&runtime, 1u);
    assert(submitted == 1u && runtime.batch_rejected == 1u);
    decode_ok = true; full = true; assert(meter_runtime_push(&runtime, &frame)); (void)meter_runtime_poll(&runtime, 1u);
    assert(submitted == 2u && runtime.diagnostics.dispatched == 1u && runtime.batch_rejected == 2u);
    builder.capacity = 1u; assert(meter_runtime_push(&runtime, &frame)); (void)meter_runtime_poll(&runtime, 1u);
    assert(submitted == 2u && runtime.batch_rejected == 3u);
    return 0;
}
