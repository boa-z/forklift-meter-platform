#include "storage/meter_nvm.h"
#include <string.h>
static meter_nvm_state_t load_state(meter_slots_result_t result)
{
    switch (result)
    {
    case METER_SLOTS_OK:
        return METER_NVM_READY;
    case METER_SLOTS_EMPTY:
        return METER_NVM_EMPTY;
    case METER_SLOTS_INCOMPATIBLE:
        return METER_NVM_INCOMPATIBLE;
    case METER_SLOTS_IO_ERROR:
        return METER_NVM_IO_ERROR;
    case METER_SLOTS_WRITE_INTERRUPTED:
        return METER_NVM_UNCERTAIN;
    default:
        return METER_NVM_CORRUPT;
    }
}
bool meter_nvm_init(meter_nvm_service_t *s, uint8_t *pending, uint8_t *inflight, size_t capacity,
                    uint16_t type, uint16_t ns, uint16_t schema, uint32_t debounce, uint32_t maximum)
{
    if (!s || !pending || !inflight || pending == inflight || !capacity || !type || !ns || !schema ||
        !maximum || debounce > maximum || maximum > INT32_MAX)
        return false;
    memset(s, 0, sizeof(*s));
    s->pending = pending;
    s->inflight = inflight;
    s->capacity = capacity;
    s->type = type;
    s->product_namespace = ns;
    s->schema = schema;
    s->generation = 1u;
    s->debounce_ms = debounce;
    s->max_delay_ms = maximum;
    s->state = METER_NVM_LOADING;
    return true;
}
static bool identity(const meter_nvm_service_t *s, const meter_record_view_t *r)
{
    return r && r->payload && r->payload_size && r->payload_size <= s->capacity && r->type == s->type &&
           r->product_namespace == s->product_namespace && r->schema == s->schema;
}
bool meter_nvm_loaded(meter_nvm_service_t *s, uint32_t generation, meter_slots_result_t result,
                      const meter_record_view_t *r)
{
    if (!s || generation != s->generation || s->state != METER_NVM_LOADING || s->observed)
        return false;
    if (result == METER_SLOTS_OK && !identity(s, r))
        result = METER_SLOTS_INCOMPATIBLE;
    s->last_result = result;
    s->state = load_state(result);
    s->writable = result == METER_SLOTS_OK || result == METER_SLOTS_EMPTY;
    if (result == METER_SLOTS_OK)
    {
        memcpy(s->pending, r->payload, r->payload_size);
        s->pending_size = r->payload_size;
        s->ram_revision = r->sequence;
        s->durable_revision = r->sequence;
        s->observed = true;
    }
    return true;
}
bool meter_nvm_observe(meter_nvm_service_t *s, const uint8_t *data, size_t n, uint32_t now)
{
    if (!s || !data || !n || n > s->capacity || s->state == METER_NVM_LOADING)
        return false;
    if (s->observed && n == s->pending_size && !memcmp(data, s->pending, n))
        return true;
    if (s->ram_revision == UINT64_MAX)
    {
        s->state = METER_NVM_FAILED;
        s->dirty = true;
        s->writable = false;
        return false;
    }
    memcpy(s->pending, data, n);
    s->pending_size = n;
    s->observed = true;
    ++s->ram_revision;
    if (!s->dirty)
        s->first_dirty_ms = now;
    s->dirty = true;
    s->last_change_ms = now;
    if (s->writable && !s->busy)
        s->state = METER_NVM_DIRTY;
    return true;
}
void meter_nvm_request_save(meter_nvm_service_t *s)
{
    if (s)
    {
        s->immediate = true;
        if (s->state == METER_NVM_CANCELLED)
            s->state = METER_NVM_DIRTY;
    }
}
bool meter_nvm_take(meter_nvm_service_t *s, uint32_t now, meter_nvm_job_t *job)
{
    if (!s || !job || !s->dirty || s->busy || !s->writable || s->state == METER_NVM_CANCELLED ||
        (!s->immediate && (uint32_t)(now - s->last_change_ms) < s->debounce_ms &&
         (uint32_t)(now - s->first_dirty_ms) < s->max_delay_ms))
        return false;
    memcpy(s->inflight, s->pending, s->pending_size);
    s->inflight_revision = s->ram_revision;
    s->busy = true;
    s->immediate = false;
    s->state = METER_NVM_IN_PROGRESS;
    *job = (meter_nvm_job_t){
        s->generation,
        s->inflight_revision,
        {s->type, s->product_namespace, s->schema, 0u, s->inflight_revision, s->inflight, s->pending_size}};
    return true;
}
bool meter_nvm_complete(meter_nvm_service_t *s, uint32_t generation, uint64_t revision,
                        meter_slots_result_t result)
{
    if (!s || !s->busy || generation != s->generation || revision != s->inflight_revision)
        return false;
    s->busy = false;
    s->last_result = result;
    if (result == METER_SLOTS_OK)
    {
        s->durable_revision = revision;
        s->dirty = s->ram_revision != revision;
        s->state = s->dirty ? METER_NVM_DIRTY : METER_NVM_DURABLE;
    }
    else
    {
        s->writable = false;
        s->state = result == METER_SLOTS_WRITE_INTERRUPTED ? METER_NVM_UNCERTAIN : METER_NVM_FAILED;
    }
    return true;
}
bool meter_nvm_reconcile(meter_nvm_service_t *s, meter_slots_result_t result, const meter_record_view_t *r)
{
    if (!s || s->busy || s->state == METER_NVM_LOADING)
        return false;
    if (result == METER_SLOTS_OK && !identity(s, r))
        result = METER_SLOTS_INCOMPATIBLE;
    s->last_result = result;
    s->writable = result == METER_SLOTS_OK || result == METER_SLOTS_EMPTY;
    if (!s->writable)
    {
        s->state = load_state(result);
        return false;
    }
    if (result == METER_SLOTS_OK)
    {
        s->durable_revision = r->sequence;
        if (s->pending_size == r->payload_size && !memcmp(s->pending, r->payload, r->payload_size))
        {
            s->ram_revision = r->sequence;
            s->dirty = false;
            s->state = METER_NVM_DURABLE;
            return true;
        }
        if (r->sequence >= s->ram_revision)
        {
            if (r->sequence == UINT64_MAX)
            {
                s->writable = false;
                s->state = METER_NVM_FAILED;
                return false;
            }
            s->ram_revision = r->sequence + 1u;
        }
    }
    else
        s->durable_revision = 0u;
    s->dirty = true;
    s->immediate = true;
    s->state = METER_NVM_DIRTY;
    return true;
}
bool meter_nvm_cancel(meter_nvm_service_t *s)
{
    if (!s || s->busy || !s->dirty || !s->writable)
        return false;
    s->state = METER_NVM_CANCELLED;
    s->immediate = false;
    return true;
}
bool meter_nvm_barrier(const meter_nvm_service_t *s, uint64_t target)
{
    return s && target != 0u && s->durable_revision >= target && s->state != METER_NVM_UNCERTAIN &&
           s->state != METER_NVM_IO_ERROR;
}
const char *meter_nvm_state_name(meter_nvm_state_t state)
{
    static const char *const names[] = {"LOADING",     "EMPTY",        "READY",    "DIRTY",
                                        "IN_PROGRESS", "DURABLE",      "FAILED",   "UNCERTAIN",
                                        "CORRUPT",     "INCOMPATIBLE", "IO_ERROR", "CANCELLED_BEFORE_IO"};
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "INVALID";
}
