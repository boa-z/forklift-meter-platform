#include "runtime/meter_runtime.h"
#include "protocols/common/meter_frame_router.h"
#include <string.h>
bool meter_runtime_init(meter_runtime_t *r, const meter_product_t *p, meter_update_sink_t sink, void *context)
{
    if (!r || !p || !p->protocols || !meter_routes_valid(p->routes) || !sink ||
        (p->protocols->count && !p->protocols->bindings))
        return false;
    for (size_t i = 0; i < p->protocols->count; ++i)
    {
        if (!p->protocols->bindings[i].decode)
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
    r->sink = sink;
    r->context = context;
    return true;
}
void meter_runtime_connection(meter_runtime_t *r, bool connected)
{
    if (r->connected != connected)
    {
        ++r->generation;
        r->head = r->tail = r->count = 0;
        r->connected = connected;
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
                if (b->decode(&q.frame, r->sink, r->context))
                    ++r->diagnostics.dispatched;
                else
                    ++r->diagnostics.decode_failed;
                break;
            }
        }
    }
    return consumed;
}
