#include "platform/rtthread/meter_rtthread_adapter.h"
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
typedef struct
{
    uint32_t now;
    bool pending;
    unsigned opened, processed, evaluated;
} board_t;
static board_t board;
static bool init(void *ctx)
{
    (void)ctx;
    return true;
}
static uint32_t now_ms(void *ctx)
{
    return ((board_t *)ctx)->now;
}
static bool open_bus(void *ctx, meter_bus_role_t bus)
{
    ++((board_t *)ctx)->opened;
    return bus == METER_BUS_CAN0;
}
static bool read_frame(void *ctx, meter_can_frame_t *frame)
{
    board_t *b = ctx;
    if (!b->pending)
        return false;
    b->pending = false;
    *frame = (meter_can_frame_t){.bus = METER_BUS_CAN0, .id = 0x123, .size = 1, .timestamp_ms = b->now};
    return true;
}
static bool decode(void *ctx, const meter_can_frame_t *f, meter_update_sink_t sink, void *sc)
{
    (void)ctx;
    meter_update_t update = {42, {17, f->timestamp_ms, METER_VALUE_VALID, 1}};
    return sink(sc, &update);
}
static bool process(void *ctx, uint32_t now, meter_update_sink_t sink, void *sc)
{
    (void)now;
    (void)sink;
    (void)sc;
    ++((board_t *)ctx)->processed;
    return true;
}
static void reset(void *ctx)
{
    (void)ctx;
}
static void evaluate(meter_snapshot_t *snapshot)
{
    (void)snapshot;
    ++board.evaluated;
}
int main(void)
{
    meter_signal_def_t signal = {42, "test.speed", "km/h", 500};
    meter_catalog_t catalog = {.signals = &signal, .signal_count = 1};
    meter_value_t value;
    meter_core_storage_t storage = {.signals = &value, .signal_capacity = 1};
    meter_core_t core;
    CHECK(meter_core_init(&core, &catalog, &storage));
    meter_protocol_adapter_t protocol = {&board, decode, process, NULL, reset};
    meter_protocol_binding_t binding = {1, NULL, &protocol};
    meter_protocol_profile_t protocols = {&binding, 1};
    meter_frame_route_t route = {METER_BUS_CAN0, 0x123, false, 1};
    meter_route_profile_t routes = {&route, 1};
    meter_product_t product = {.id = "rtthread-test",
                               .protocols = &protocols,
                               .routes = &routes,
                               .catalog = &catalog,
                               .evaluate = evaluate};
    meter_rtthread_board_port_t port = {.display_init = init,
                                        .touch_init = init,
                                        .can_open = open_bus,
                                        .can_read = read_frame,
                                        .context = &board,
                                        .now_ms = now_ms};
    meter_rtthread_adapter_t adapter;
    port.now_ms = NULL;
    CHECK(!meter_rtthread_adapter_init(&adapter, &product, &core, &port));
    port.now_ms = now_ms;
    CHECK(meter_rtthread_adapter_init(&adapter, &product, &core, &port));
    CHECK(board.opened == 1);
    board.now = UINT32_MAX - 100U;
    board.pending = true;
    CHECK(meter_rtthread_adapter_poll(&adapter, 8));
    CHECK(value.state == METER_VALUE_VALID && value.value == 17);
    board.now = 398; /* 499 ms after sample, across uint32 wrap. */
    CHECK(meter_rtthread_adapter_poll(&adapter, 8));
    CHECK(value.state == METER_VALUE_VALID);
    board.now = 399;
    CHECK(meter_rtthread_adapter_poll(&adapter, 8));
    CHECK(value.state == METER_VALUE_STALE);
    CHECK(board.processed == 3 && board.evaluated == 3);
    board.pending = true;
    CHECK(meter_rtthread_adapter_poll(&adapter, 8));
    CHECK(value.state == METER_VALUE_VALID);
    meter_rtthread_adapter_disconnect(&adapter);
    CHECK(!core.snapshot.connected && value.state == METER_VALUE_STALE);
    CHECK(!meter_rtthread_adapter_poll(&adapter, 8));
    CHECK(adapter.runtime.diagnostics.dispatched == 2);
    return 0;
}
