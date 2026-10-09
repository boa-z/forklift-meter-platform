#include "runtime/meter_runtime.h"
#include "protocols/common/meter_frame_router.h"
#include <string.h>

bool meter_runtime_bind_batches(meter_runtime_t *r, meter_batch_builder_t *b,
                                meter_batch_submit_fn_t submit, void *context)
{
    if (!r || !b || !b->storage || !b->capacity || !submit || r->connected) return false;
    r->batch_builder = b;
    r->batch_submit = submit;
    r->batch_context = context;
    return true;
}
static bool batch_update(void *context, const meter_update_t *update)
{
    meter_batch_builder_t *b = context;
    if (update && b->batch.count == 0u) b->batch.source = update->value.source;
    return meter_batch_builder_add(b, update);
}
static bool batch_begin(meter_runtime_t *r, uint32_t now, uint8_t bus)
{
    if (!r->batch_builder) return true;
    if (r->batch_sequence == UINT64_MAX) return false;
    meter_update_batch_t metadata = {.generation = r->generation, .sequence = ++r->batch_sequence,
        .received_ms = now, .bus = bus, .source = METER_SOURCE_DEMO};
    return meter_batch_builder_begin(r->batch_builder, &metadata);
}
static bool batch_finish(meter_runtime_t *r, bool decoded)
{
    if (!r->batch_builder) return decoded;
    meter_batch_result_t result = meter_batch_builder_finish(r->batch_builder, decoded,
                                                            r->batch_submit, r->batch_context);
    if (result != METER_BATCH_QUEUED) meter_diag_increment(&r->batch_rejected);
    return result == METER_BATCH_QUEUED;
}

static void diag_sync(meter_runtime_t *r)
{
    if (!r || !r->diag)
        return;
    r->diag->data.runtime = (meter_diag_runtime_t){
        true,          r->connected, r->generation, r->count, METER_RX_CAPACITY, r->product->protocols->count,
        r->diagnostics};
}
void meter_runtime_bind_diagnostics(meter_runtime_t *r, meter_diagnostics_t *d)
{
    if (r)
    {
        r->diag = d;
        diag_sync(r);
    }
}
static void runtime_services(const meter_runtime_t *r, const meter_can_tx_port_t *tx,
                             meter_protocol_services_t *services)
{
    services->diagnostics = r->diag;
    services->update = r->batch_builder ? batch_update : r->update;
    services->update_context = r->batch_builder ? r->batch_builder : r->update_context;
    services->event = r->event_sink;
    services->event_context = r->event_context;
    services->tx = tx ? tx : r->tx;
    services->request = r->command_request;
    services->progress = r->command_progress;
    services->progress_context = r->command_progress_context;
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
        meter_diag_increment(&r->diagnostics.resets);
        uint32_t now = r->diag ? r->diag->data.uptime_ms : 0;
        METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, connected ? RUNTIME_CONNECT : RUNTIME_DISCONNECT, now,
                         r->generation, 0);
        METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_RESET, now, r->generation, (uint32_t)r->count);
        r->head = r->tail = r->count = 0;
        r->connected = connected;
        if (!connected)
            for (size_t i = 0; i < r->product->protocols->count; ++i)
                if (r->product->protocols->bindings[i].adapter &&
                    r->product->protocols->bindings[i].adapter->reset)
                    r->product->protocols->bindings[i].adapter->reset(
                        r->product->protocols->bindings[i].adapter->context);
    }
    diag_sync(r);
}
bool meter_runtime_session(meter_runtime_t *r, uint32_t generation)
{
    if (!r || !r->product || !r->product->protocols || !generation) return false;
    if (r->connected && r->generation == generation) return true;
    for (size_t i = 0u; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_adapter_t *a = r->product->protocols->bindings[i].adapter;
        if (a && a->reset) a->reset(a->context);
    }
    r->head = r->tail = r->count = 0u;
    r->generation = generation;
    r->connected = true;
    meter_diag_increment(&r->diagnostics.resets);
    uint32_t now = r->diag ? r->diag->data.uptime_ms : 0u;
    METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_CONNECT, now, generation, 0u);
    METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_RESET, now, generation, 0u);
    diag_sync(r);
    return true;
}
bool meter_runtime_push(meter_runtime_t *r, const meter_can_frame_t *f)
{
    if (!r)
        return false;
    if (f)
        meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX, f->timestamp_ms);
    if (!f || !meter_frame_valid(f))
    {
        ++r->diagnostics.malformed;
        if (f)
            meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX_ERROR, f->timestamp_ms);
        diag_sync(r);
        return false;
    }
    if (!r->connected)
    {
        meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX_DROP, f->timestamp_ms);
        return false;
    }
    if (r->count == METER_RX_CAPACITY)
    {
        ++r->diagnostics.overflow;
        meter_diagnostics_can(r->diag, f->bus, METER_CAN_RX_DROP, f->timestamp_ms);
        METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_QUEUE_OVERFLOW, f->timestamp_ms, f->bus,
                         (uint32_t)r->count);
        diag_sync(r);
        return false;
    }
    r->queue[r->tail].frame = *f;
    r->queue[r->tail].generation = r->generation;
    r->tail = (r->tail + 1) % METER_RX_CAPACITY;
    ++r->count;
    ++r->diagnostics.accepted;
    diag_sync(r);
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
                if (!batch_begin(r, q.frame.timestamp_ms, (uint8_t)q.frame.bus))
                {
                    meter_diag_increment(&r->batch_rejected);
                    break;
                }
                if (b->adapter)
                {
                    meter_protocol_services_t services;
                    runtime_services(r, NULL, &services);
                    handled = b->adapter->on_frame(b->adapter->context, &q.frame, &services);
                }
                else
                {
                    handled = b->decode(&q.frame, r->batch_builder ? batch_update : r->update,
                                         r->batch_builder ? r->batch_builder : r->update_context);
                }
                handled = batch_finish(r, handled);
                if (handled)
                    ++r->diagnostics.dispatched;
                else
                {
                    ++r->diagnostics.decode_failed;
                    METER_DIAG_TRACE(r->diag, METER_TRACE_RUNTIME, RUNTIME_INVALID_FRAME,
                                     q.frame.timestamp_ms, q.frame.id, q.frame.bus);
                }
                break;
            }
        }
    }
    diag_sync(r);
    return consumed;
}
bool meter_runtime_process(meter_runtime_t *r, uint32_t now_ms)
{
    if (!r || !r->connected || !r->product || !r->product->protocols)
        return false;
    meter_diagnostics_time(r->diag, now_ms);
    bool ok = true;
    for (size_t i = 0; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_binding_t *b = &r->product->protocols->bindings[i];
        if (b->adapter && b->adapter->process)
        {
            meter_protocol_services_t services;
            runtime_services(r, NULL, &services);
            if (!batch_begin(r, now_ms, 0u))
                ok = false;
            else if (!batch_finish(r, b->adapter->process(b->adapter->context, now_ms, &services)))
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
            if (!batch_begin(r, r->diag ? r->diag->data.uptime_ms : 0u, 0u)) return false;
            return batch_finish(r, b->adapter->command(b->adapter->context, command, &services));
        }
    }
    return false;
}
bool meter_runtime_command_request(meter_runtime_t *r, const meter_command_t *command,
                                   meter_request_id_t id, meter_command_progress_fn_t progress, void *context)
{
    if (!r || !id.session || !id.serial || !progress || r->command_request.serial) return false;
    r->command_request = id;
    r->command_progress = progress;
    r->command_progress_context = context;
    bool accepted = meter_runtime_command(r, command);
    r->command_request = (meter_request_id_t){0};
    progress(context, id, accepted ? METER_COMMAND_APPLIED : METER_COMMAND_FAILED);
    return accepted;
}

void meter_runtime_cancel(meter_runtime_t *r, meter_request_id_t request)
{
    if (!r || !request.serial) return;
    for (size_t i = 0u; i < r->product->protocols->count; ++i)
    {
        const meter_protocol_adapter_t *adapter = r->product->protocols->bindings[i].adapter;
        if (adapter && adapter->cancel) adapter->cancel(adapter->context, request);
    }
}
