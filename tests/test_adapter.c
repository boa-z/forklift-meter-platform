#include "core/meter_core.h"
#include "runtime/meter_runtime.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct { unsigned frames, processes, commands, resets, sends; void *sink_context; } state_t;
static bool on_frame(void *ctx, const meter_can_frame_t *f, meter_update_sink_t sink, void *sc)
{
    state_t *a = ctx;
    ++a->frames;
    a->sink_context = sc;
    meter_update_t u = {42, {17, f->timestamp_ms, METER_VALUE_VALID, 7}};
    return sink(sc, &u);
}
static bool process(void *ctx, uint32_t now, meter_update_sink_t sink, void *sc)
{
    ++((state_t *)ctx)->processes;
    meter_update_t u = {42, {63, now, METER_VALUE_VALID, 7}};
    return sink(sc, &u);
}
static bool send(void *ctx, const meter_can_frame_t *f)
{
    ++((state_t *)ctx)->sends;
    return f->id == 0x123;
}
static bool command(void *ctx, const meter_command_t *cmd, const meter_can_tx_port_t *tx)
{
    ++((state_t *)ctx)->commands;
    meter_can_frame_t f = {.bus=METER_BUS_CAN0, .id=cmd->id, .size=1};
    return tx->send(tx->context, &f);
}
static void reset(void *ctx) { ++((state_t *)ctx)->resets; }
int main(void)
{
    state_t a = {0};
    meter_signal_def_t signal = {42, "test.speed", "km/h", 500};
    meter_catalog_t catalog = {.signals=&signal, .signal_count=1};
    meter_value_t value;
    meter_core_storage_t storage = {.signals=&value, .signal_capacity=1};
    meter_core_t core;
    CHECK(meter_core_init(&core, &catalog, &storage));
    meter_protocol_adapter_t adapter = {&a, on_frame, process, command, reset};
    meter_protocol_binding_t binding = {1, NULL, &adapter};
    meter_protocol_profile_t protocols = {&binding, 1};
    meter_frame_route_t route = {METER_BUS_CAN0, 0x123, false, 1};
    meter_route_profile_t routes = {&route, 1};
    meter_product_t product = {.id="adapter-test", .protocols=&protocols, .routes=&routes, .catalog=&catalog};
    meter_runtime_t runtime;
    CHECK(meter_runtime_init(&runtime, &product, meter_core_apply, &core));
    meter_can_tx_port_t tx = {send, &a};
    meter_command_t cmd = {0x123, 0, 0};
    CHECK(!meter_runtime_process(&runtime, 10));
    CHECK(!meter_runtime_command(&runtime, 1, &cmd, &tx));
    meter_runtime_connection(&runtime, true);
    meter_can_frame_t f = {.bus=METER_BUS_CAN0, .id=0x123, .size=1};
    CHECK(meter_runtime_push(&runtime, &f));
    CHECK(meter_runtime_poll(&runtime, 1) == 1);
    CHECK(a.frames == 1 && a.sink_context == &core && a.sink_context != &a);
    CHECK(value.value == 17 && value.source == 7);
    CHECK(meter_runtime_process(&runtime, 123));
    CHECK(a.processes == 1 && value.timestamp_ms == 123 && value.value == 63);
    CHECK(meter_runtime_command(&runtime, 1, &cmd, &tx));
    CHECK(a.commands == 1 && a.sends == 1);
    cmd.id = 0x124;
    CHECK(!meter_runtime_command(&runtime, 1, &cmd, &tx));
    CHECK(!meter_runtime_command(&runtime, 99, &cmd, &tx));
    CHECK(!meter_runtime_command(&runtime, 1, &cmd, NULL));
    CHECK(meter_runtime_push(&runtime, &f));
    meter_runtime_connection(&runtime, false);
    meter_runtime_connection(&runtime, false);
    CHECK(a.resets == 1 && runtime.count == 0);
    CHECK(!meter_runtime_command(&runtime, 1, &cmd, &tx));
    meter_runtime_connection(&runtime, true);
    CHECK(meter_runtime_poll(&runtime, 32) == 0);
    adapter.on_frame = NULL;
    CHECK(!meter_runtime_init(&runtime, &product, meter_core_apply, &core));
    return 0;
}
