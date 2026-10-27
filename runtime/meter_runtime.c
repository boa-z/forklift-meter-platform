#include "runtime/meter_runtime.h"
#include "protocols/common/meter_frame_router.h"
#include <string.h>

static void diag_sync(meter_runtime_t *r)
{
    if (r->diag)
        r->diag->data.runtime = (meter_diag_runtime_t){true, r->connected, r->generation,
                                                       r->product->protocols->count, r->diagnostics};
}
static meter_protocol_services_t services_of(const meter_runtime_t *r)
{
    return (meter_protocol_services_t){r->update, r->update_context, r->tx, r->diag};
}
static const meter_protocol_binding_t *binding_of(const meter_runtime_t *r, meter_frame_route_owner_t owner)
{
    for (size_t i = 0; i < r->product->protocols->count; ++i)
        if (r->product->protocols->bindings[i].owner == owner)
            return &r->product->protocols->bindings[i];
    return NULL;
}
static void reset_adapters(const meter_runtime_t *r)
{
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_adapter_t *a = r->product->protocols->bindings[i].adapter;
        if (a && a->reset)
            a->reset(a->context);
    }
}
bool meter_runtime_init(meter_runtime_t *r, const meter_product_t *p, meter_update_sink_t update,
                        void *update_context)
{
    if (!r || !p || !p->protocols || !meter_routes_valid(p->routes) || !update ||
        (p->protocols->count && !p->protocols->bindings))
        return false;
    for (size_t i = 0; i < p->protocols->count; ++i)
    {
        const meter_protocol_binding_t *b = &p->protocols->bindings[i];
        if ((!b->decode && !b->adapter) || (b->adapter && !b->adapter->on_frame))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (b->owner == p->protocols->bindings[j].owner)
                return false;
    }
    meter_runtime_t probe = {.product = p};
    for (size_t i = 0; i < p->routes->count; ++i)
        if (!binding_of(&probe, p->routes->entries[i].owner))
            return false;
    memset(r, 0, sizeof(*r));
    r->product = p;
    r->update = update;
    r->update_context = update_context;
    return true;
}
void meter_runtime_bind_tx(meter_runtime_t *r, const meter_can_tx_port_t *tx)
{
    if (r)
        r->tx = tx;
}
void meter_runtime_bind_diagnostics(meter_runtime_t *r, meter_diagnostics_t *d)
{
    if (r)
    {
        r->diag = d;
        diag_sync(r);
    }
}
void meter_runtime_connection(meter_runtime_t *r, bool connected)
{
    if (r->connected != connected)
    {
        ++r->generation;
        meter_diag_increment(&r->diagnostics.resets);
        uint32_t now = r->diag ? r->diag->data.uptime_ms : 0;
        METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, connected ? RUNTIME_CONNECT : RUNTIME_DISCONNECT, now,
                         r->generation, 0);
        r->connected = connected;
        if (!connected)
            reset_adapters(r);
    }
    diag_sync(r);
}
bool meter_runtime_receive(meter_runtime_t *r, const meter_can_frame_t *f)
{
    if (!r)
        return false;
    if (f)
        meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX, f->timestamp_ms);
    bool handled = false;
    if (!f || !meter_frame_valid(f))
    {
        ++r->diagnostics.malformed;
        if (f)
            meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX_ERROR, f->timestamp_ms);
    }
    else if (!r->connected)
        meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX_DROP, f->timestamp_ms);
    else
    {
        ++r->diagnostics.accepted;
        const meter_frame_route_t *route = meter_route_lookup(r->product->routes, f);
        const meter_protocol_binding_t *b = route ? binding_of(r, route->owner) : NULL;
        if (!b)
            ++r->diagnostics.unrouted;
        else
        {
            if (b->adapter)
            {
                const meter_protocol_services_t services = services_of(r);
                handled = b->adapter->on_frame(b->adapter->context, f, &services);
            }
            else
                handled = b->decode(f, r->update, r->update_context);
            if (handled)
                ++r->diagnostics.dispatched;
            else
            {
                ++r->diagnostics.decode_failed;
                METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_INVALID_FRAME, f->timestamp_ms, f->id,
                                 f->bus);
            }
        }
    }
    diag_sync(r);
    return handled;
}
bool meter_runtime_process(meter_runtime_t *r, uint32_t now_ms)
{
    if (!r || !r->connected)
        return false;
    meter_diagnostics_time(r->diag, now_ms);
    const meter_protocol_services_t services = services_of(r);
    bool ok = true;
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_adapter_t *a = r->product->protocols->bindings[i].adapter;
        if (a && a->process && !a->process(a->context, now_ms, &services))
            ok = false;
    }
    return ok;
}
bool meter_runtime_command(meter_runtime_t *r, const meter_command_t *command)
{
    meter_frame_route_owner_t owner = 0;
    if (!r || !r->connected || !command || !r->product->command_route ||
        !r->product->command_route(r->product->command_route_context, command, &owner))
        return false;
    const meter_protocol_binding_t *b = binding_of(r, owner);
    if (!b || !b->adapter || !b->adapter->command)
        return false;
    const meter_protocol_services_t services = services_of(r);
    return b->adapter->command(b->adapter->context, command, &services);
}
static bool port_send(void *context, const meter_command_t *command)
{
    return meter_runtime_command(context, command);
}
meter_command_port_t meter_runtime_command_port(meter_runtime_t *r)
{
    return (meter_command_port_t){r, port_send};
}
void meter_runtime_step(meter_runtime_t *r, meter_core_t *core, uint32_t now_ms, bool commands)
{
    const meter_product_t *p = r->product;
    if (commands)
    {
        (void)meter_runtime_process(r, now_ms);
        if (p->app_run)
        {
            const meter_command_port_t port = meter_runtime_command_port(r);
            p->app_run(now_ms, &port);
        }
    }
    meter_core_tick(core, now_ms);
    if (p->app_tick)
        p->app_tick(&core->snapshot, now_ms);
}
