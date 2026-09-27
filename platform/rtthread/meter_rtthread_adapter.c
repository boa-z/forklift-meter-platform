#include "platform/rtthread/meter_rtthread_adapter.h"
#include <string.h>
struct meter_rtthread_adapter
{
    meter_runtime_t runtime;
    meter_core_t *core;
    meter_rtthread_board_port_t board;
};
bool meter_rtthread_adapter_init(meter_rtthread_adapter_t *a, const meter_product_t *p, meter_core_t *core,
                                 const meter_rtthread_board_port_t *b)
{
    if (!a || !p || !core || !b || !b->display_init || !b->touch_init || !b->can_open || !b->can_read ||
        !b->display_init(b->context) || !b->touch_init(b->context) ||
        !b->can_open(b->context, METER_BUS_CAN0))
        return false;
    memset(a, 0, sizeof(*a));
    a->core = core;
    a->board = *b;
    if (!meter_runtime_init(&a->runtime, p, meter_core_apply, core))
        return false;
    meter_runtime_connection(&a->runtime, true);
    return true;
}
bool meter_rtthread_adapter_poll(meter_rtthread_adapter_t *a, size_t budget)
{
    if (!a || !a->runtime.connected)
        return false;
    meter_can_frame_t f = {0};
    size_t reads = 0;
    while (reads < budget && a->board.can_read(a->board.context, &f))
    {
        (void)meter_runtime_push(&a->runtime, &f);
        ++reads;
    }
    meter_runtime_poll(&a->runtime, budget);
    meter_core_connection(a->core, a->runtime.connected, a->runtime.generation);
    meter_core_tick(a->core, f.timestamp_ms, 750);
    return true;
}
void meter_rtthread_adapter_disconnect(meter_rtthread_adapter_t *a)
{
    if (a)
        meter_runtime_connection(&a->runtime, false);
}
const meter_runtime_diagnostics_t *meter_rtthread_adapter_diagnostics(const meter_rtthread_adapter_t *a)
{
    return a ? &a->runtime.diagnostics : NULL;
}
