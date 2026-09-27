#include "runtime/meter_runtime.h"
#include "protocols/common/meter_frame_router.h"
#include <string.h>

static void runtime_services(const meter_runtime_t *r, const meter_can_tx_port_t *tx,
                             meter_protocol_services_t *services)
{
    services->update = r->update;
    services->update_context = r->update_context;
    services->event = r->event_sink;
    services->event_context = r->event_context;
    services->tx = tx ? tx : r->tx;
}

bool meter_runtime_init(meter_runtime_t *r, const meter_product_t *p, meter_update_sink_t update,
                        void *update_context)
{
    if (!r || !p || !p->protocols || !meter_routes_valid(p->routes) || !update ||
        (p->protocols->count && !p->protocols->bindings))
        return false;
    for (size_t i = 0; i < p->protocols->count; ++i)
    {
        if ((!p->protocols->bindings[i].decode && !p->protocols->bindings[i].adapter) ||
            (p->protocols->bindings[i].adapter && !p->protocols->bindings[i].adapter->on_frame))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (p->protocols->bindings[i].owner == p->protocols->bindings[j].owner)
                return false;
    }
    for (size_t i = 0; i < p->routes->count; ++i)
    {
        bool found = false;
        for (size_t j = 0; j < p->protocols->count; ++j)
            if (p->routes->entries[i].owner == p->protocols->bindings[j].owner)
                found = true;
        if (!found)
            return false;
    }
    memset(r, 0, sizeof(*r));
    r->product = p;
    r->update = update;
    r->update_context = update_context;
    return true;
}
void meter_runtime_bind_services(meter_runtime_t *r, meter_protocol_event_sink_t event_sink,
                                 void *event_context, const meter_can_tx_port_t *tx)
{
    if (!r)
        return;
    r->event_sink = event_sink;
    r->event_context = event_context;
    r->tx = tx;
}
void meter_runtime_connection(meter_runtime_t *r, bool connected)
{
    if (r->connected != connected)
    {
        ++r->generation;
        r->head = r->tail = r->count = 0;
        r->connected = connected;
        if (!connected)
            for (size_t i = 0; i < r->product->protocols->count; ++i)
                if (r->product->protocols->bindings[i].adapter && r->product->protocols->bindings[i].adapter->reset)
                    r->product->protocols->bindings[i].adapter->reset(
                        r->product->protocols->bindings[i].adapter->context);
    }
}
bool meter_runtime_push(meter_runtime_t *r, const meter_can_frame_t *f)
{
    if (!meter_frame_valid(f))
    {
        ++r->diagnostics.malformed;
        return false;
    }
    if (!r->connected)
        return false;
    if (r->count == METER_RX_CAPACITY)
    {
        ++r->diagnostics.overflow;
        return false;
    }
    r->queue[r->tail].frame = *f;
    r->queue[r->tail].generation = r->generation;
    r->tail = (r->tail + 1) % METER_RX_CAPACITY;
    ++r->count;
    ++r->diagnostics.accepted;
    return true;
}
size_t meter_runtime_poll(meter_runtime_t *r, size_t budget)
{
    size_t consumed = 0;
    while (r->count && consumed < budget)
    {
        meter_queued_frame_t q = r->queue[r->head];
        r->head = (r->head + 1) % METER_RX_CAPACITY;
        --r->count;
        ++consumed;
        if (!r->connected || q.generation != r->generation)
            continue;
        const meter_frame_route_t *route = meter_route_lookup(r->product->routes, &q.frame);
        if (!route)
        {
            ++r->diagnostics.unrouted;
            continue;
        }
        for (size_t i = 0; i < r->product->protocols->count; ++i)
        {
            const meter_protocol_binding_t *b = &r->product->protocols->bindings[i];
            if (b->owner == route->owner)
            {
                bool handled;
                if (b->adapter)
                {
                    meter_protocol_services_t services;
                    runtime_services(r, NULL, &services);
                    handled = b->adapter->on_frame(b->adapter->context, &q.frame, &services);
                }
                else
                {
                    handled = b->decode(&q.frame, r->update, r->update_context);
                }
                if (handled)
                    ++r->diagnostics.dispatched;
                else
                    ++r->diagnostics.decode_failed;
                break;
            }
        }
    }
    return consumed;
}
bool meter_runtime_process(meter_runtime_t *r, uint32_t now_ms)
{
    if (!r || !r->connected || !r->product || !r->product->protocols)
        return false;
    bool ok = true;
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_binding_t *b = &r->product->protocols->bindings[i];
        if (b->adapter && b->adapter->process)
        {
            meter_protocol_services_t services;
            runtime_services(r, NULL, &services);
            if (!b->adapter->process(b->adapter->context, now_ms, &services))
                ok = false;
        }
    }
    return ok;
}
bool meter_runtime_command(meter_runtime_t *r, const meter_command_t *command)
{
    if (!r || !r->connected || !command || !r->product || !r->product->protocols)
        return false;
    if (!r->tx || !r->tx->send)
        return false;
    if (!r->product->command_route)
        return false;
    meter_frame_route_owner_t owner = 0;
    if (!r->product->command_route(r->product->command_route_context, command, &owner))
        return false;
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_binding_t *b = &r->product->protocols->bindings[i];
        if (b->owner == owner)
        {
            if (!b->adapter || !b->adapter->command)
                return false;
            meter_protocol_services_t services;
            runtime_services(r, NULL, &services);
            return b->adapter->command(b->adapter->context, command, &services);
        }
    }
    return false;
}
