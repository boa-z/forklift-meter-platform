#include "runtime/hmi_runtime.h"
#include "protocols/common/hmi_frame_router.h"
#include <string.h>

static void diag_sync(hmi_runtime_t *r)
{
    if (r->diag)
        r->diag->data.runtime = (hmi_diag_runtime_t){true, r->connected, r->generation,
                                                       r->product->protocols->count, r->diagnostics};
}
static hmi_protocol_services_t services_of(const hmi_runtime_t *r)
{
    return (hmi_protocol_services_t){r->update, r->update_context, r->tx, r->diag};
}
static const hmi_protocol_binding_t *binding_of(const hmi_runtime_t *r, hmi_frame_route_owner_t owner)
{
    for (size_t i = 0; i < r->product->protocols->count; ++i)
        if (r->product->protocols->bindings[i].owner == owner)
            return &r->product->protocols->bindings[i];
    return NULL;
}
static void reset_adapters(const hmi_runtime_t *r)
{
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const hmi_protocol_adapter_t *a = r->product->protocols->bindings[i].adapter;
        if (a && a->reset)
            a->reset(a->context);
    }
}
bool hmi_runtime_init(hmi_runtime_t *r, const hmi_product_t *p, hmi_update_sink_t update,
                        void *update_context)
{
    if (!r || !p || !p->protocols || !hmi_routes_valid(p->routes) || !update ||
        (p->protocols->count && !p->protocols->bindings))
        return false;
    for (size_t i = 0; i < p->protocols->count; ++i)
    {
        const hmi_protocol_binding_t *b = &p->protocols->bindings[i];
        if ((!b->decode && !b->adapter) || (b->adapter && !b->adapter->on_frame))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (b->owner == p->protocols->bindings[j].owner)
                return false;
    }
    hmi_runtime_t probe = {.product = p};
    for (size_t i = 0; i < p->routes->count; ++i)
        if (!binding_of(&probe, p->routes->entries[i].owner))
            return false;
    memset(r, 0, sizeof(*r));
    r->product = p;
    r->update = update;
    r->update_context = update_context;
    return true;
}
void hmi_runtime_bind_tx(hmi_runtime_t *r, const hmi_can_tx_port_t *tx)
{
    if (r)
        r->tx = tx;
}
void hmi_runtime_bind_diagnostics(hmi_runtime_t *r, hmi_diagnostics_t *d)
{
    if (r)
    {
        r->diag = d;
        diag_sync(r);
    }
}
void hmi_runtime_connection(hmi_runtime_t *r, bool connected)
{
    if (r->connected != connected)
    {
        ++r->generation;
        hmi_diag_increment(&r->diagnostics.resets);
        uint32_t now = r->diag ? r->diag->data.uptime_ms : 0;
        HMI_DIAG_TRACE(r->diag, HMI_TRACE_RUNTIME, connected ? RUNTIME_CONNECT : RUNTIME_DISCONNECT, now,
                         r->generation, 0);
        r->connected = connected;
        if (!connected)
            reset_adapters(r);
    }
    diag_sync(r);
}
bool hmi_runtime_receive(hmi_runtime_t *r, const hmi_can_frame_t *f)
{
    if (!r)
        return false;
    if (f)
        hmi_diagnostics_can(r->diag, f->bus, HMI_CAN_RX, f->timestamp_ms);
    bool handled = false;
    if (!f || !hmi_frame_valid(f))
    {
        ++r->diagnostics.malformed;
        if (f)
            hmi_diagnostics_can(r->diag, f->bus, HMI_CAN_RX_ERROR, f->timestamp_ms);
    }
    else if (!r->connected)
        hmi_diagnostics_can(r->diag, f->bus, HMI_CAN_RX_DROP, f->timestamp_ms);
    else
    {
        ++r->diagnostics.accepted;
        const hmi_frame_route_t *route = hmi_route_lookup(r->product->routes, f);
        const hmi_protocol_binding_t *b = route ? binding_of(r, route->owner) : NULL;
        if (!b)
            ++r->diagnostics.unrouted;
        else
        {
            if (b->adapter)
            {
                const hmi_protocol_services_t services = services_of(r);
                handled = b->adapter->on_frame(b->adapter->context, f, &services);
            }
            else
                handled = b->decode(f, r->update, r->update_context);
            if (handled)
                ++r->diagnostics.dispatched;
            else
            {
                ++r->diagnostics.decode_failed;
                HMI_DIAG_TRACE(r->diag, HMI_TRACE_RUNTIME, RUNTIME_INVALID_FRAME, f->timestamp_ms, f->id,
                                 f->bus);
            }
        }
    }
    diag_sync(r);
    return handled;
}
bool hmi_runtime_process(hmi_runtime_t *r, uint32_t now_ms)
{
    if (!r || !r->connected)
        return false;
    hmi_diagnostics_time(r->diag, now_ms);
    const hmi_protocol_services_t services = services_of(r);
    bool ok = true;
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const hmi_protocol_adapter_t *a = r->product->protocols->bindings[i].adapter;
        if (a && a->process && !a->process(a->context, now_ms, &services))
            ok = false;
    }
    return ok;
}
bool hmi_runtime_command(hmi_runtime_t *r, const hmi_command_t *command)
{
    hmi_frame_route_owner_t owner = 0;
    if (!r || !r->connected || !command || !r->product->command_route ||
        !r->product->command_route(r->product->command_route_context, command, &owner))
        return false;
    const hmi_protocol_binding_t *b = binding_of(r, owner);
    if (!b || !b->adapter || !b->adapter->command)
        return false;
    const hmi_protocol_services_t services = services_of(r);
    return b->adapter->command(b->adapter->context, command, &services);
}
static bool port_send(void *context, const hmi_command_t *command)
{
    return hmi_runtime_command(context, command);
}
hmi_command_port_t hmi_runtime_command_port(hmi_runtime_t *r)
{
    return (hmi_command_port_t){r, port_send};
}
void hmi_runtime_step(hmi_runtime_t *r, hmi_core_t *core, uint32_t now_ms, bool commands)
{
    const hmi_product_t *p = r->product;
    if (commands)
    {
        (void)hmi_runtime_process(r, now_ms);
        if (p->app_run)
        {
            const hmi_command_port_t port = hmi_runtime_command_port(r);
            p->app_run(now_ms, &port);
        }
    }
    hmi_core_tick(core, now_ms);
    if (p->app_tick)
        p->app_tick(&core->snapshot, now_ms);
}
