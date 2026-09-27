#include "platform/rtthread/meter_rtthread_adapter.h"
#include <string.h>
bool meter_rtthread_adapter_init(meter_rtthread_adapter_t *a, const meter_product_t *p, meter_core_t *core,
                                 const meter_rtthread_board_port_t *b)
{
    if (!a || !p || !p->routes || !core || !b || !b->now_ms || !b->display_init || !b->touch_init ||
        !b->can_open || !b->can_read || !b->display_init(b->context) || !b->touch_init(b->context))
        return false;
    memset(a, 0, sizeof(*a));
    a->core = core;
    a->board = *b;
    bool can0 = false, can1 = false;
    for (size_t i = 0; i < p->routes->count; ++i)
    {
        can0 |= p->routes->entries[i].bus == METER_BUS_CAN0;
        can1 |= p->routes->entries[i].bus == METER_BUS_CAN1;
    }
    if ((can0 && !b->can_open(b->context, METER_BUS_CAN0)) ||
        (can1 && !b->can_open(b->context, METER_BUS_CAN1)))
        return false;
    if (!meter_runtime_init(&a->runtime, p, meter_core_apply, core))
        return false;
    meter_runtime_bind_diagnostics(&a->runtime, core->diag);
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
    uint32_t now = a->board.now_ms(a->board.context);
    (void)meter_runtime_process(&a->runtime, now);
    meter_core_tick(a->core, now);
    if (a->runtime.product->evaluate)
        a->runtime.product->evaluate(&a->core->snapshot);
    return true;
}
void meter_rtthread_adapter_disconnect(meter_rtthread_adapter_t *a)
{
    if (a)
    {
        meter_runtime_connection(&a->runtime, false);
        meter_core_connection(a->core, false, a->runtime.generation);
    }
}
const meter_runtime_diagnostics_t *meter_rtthread_adapter_diagnostics(const meter_rtthread_adapter_t *a)
{
    return a ? &a->runtime.diagnostics : NULL;
}
