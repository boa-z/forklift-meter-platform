#include "storage/hmi_nvm.h"
#include <string.h>
static hmi_nvm_state_t load_state(hmi_slots_result_t result)
{
    switch (result)
    {
    case HMI_SLOTS_OK:
        return HMI_NVM_READY;
    case HMI_SLOTS_EMPTY:
        return HMI_NVM_EMPTY;
    case HMI_SLOTS_INCOMPATIBLE:
        return HMI_NVM_INCOMPATIBLE;
    case HMI_SLOTS_IO_ERROR:
        return HMI_NVM_IO_ERROR;
    case HMI_SLOTS_WRITE_INTERRUPTED:
        return HMI_NVM_UNCERTAIN;
    default:
        return HMI_NVM_CORRUPT;
    }
}
bool hmi_nvm_init(hmi_nvm_service_t *s, uint8_t *pending, uint8_t *inflight, size_t capacity,
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
    s->state = HMI_NVM_LOADING;
    return true;
}
static bool identity(const hmi_nvm_service_t *s, const hmi_record_view_t *r)
{
    return r && r->payload && r->payload_size && r->payload_size <= s->capacity && r->type == s->type &&
           r->product_namespace == s->product_namespace && r->schema == s->schema;
}
bool hmi_nvm_loaded(hmi_nvm_service_t *s, uint32_t generation, hmi_slots_result_t result,
                      const hmi_record_view_t *r)
{
    if (!s || generation != s->generation || s->state != HMI_NVM_LOADING || s->observed)
        return false;
    if (result == HMI_SLOTS_OK && !identity(s, r))
        result = HMI_SLOTS_INCOMPATIBLE;
    s->last_result = result;
    s->state = load_state(result);
    s->writable = s->always_writable || result == HMI_SLOTS_OK || result == HMI_SLOTS_EMPTY;
    if (result == HMI_SLOTS_OK)
    {
        memcpy(s->pending, r->payload, r->payload_size);
        s->pending_size = r->payload_size;
        s->ram_revision = r->sequence;
        s->durable_revision = r->sequence;
        s->observed = true;
    }
    else if (s->always_writable && r && r->sequence != UINT64_MAX)
    {
        /* 介质上有用不了的记录（payload 不兼容、同代次冲突）：从见到的最大序号往后编，
           替代记录才会在下次扫描时被选中。 */
        s->ram_revision = r->sequence;
    }
    return true;
}
bool hmi_nvm_observe(hmi_nvm_service_t *s, const uint8_t *data, size_t n, uint32_t now)
{
    if (!s || !data || !n || n > s->capacity || s->state == HMI_NVM_LOADING)
        return false;
    if (s->observed && n == s->pending_size && !memcmp(data, s->pending, n))
        return true;
    if (s->ram_revision == UINT64_MAX)
    {
        s->state = HMI_NVM_FAILED;
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
        s->state = HMI_NVM_DIRTY;
    return true;
}
void hmi_nvm_request_save(hmi_nvm_service_t *s)
{
    if (s)
    {
        s->immediate = true;
        if (s->state == HMI_NVM_CANCELLED)
            s->state = HMI_NVM_DIRTY;
    }
}
bool hmi_nvm_take(hmi_nvm_service_t *s, uint32_t now, hmi_nvm_job_t *job)
{
    /* always_writable 下失败后的状态（FAILED/UNCERTAIN）留给诊断，不挡下一次提交。 */
    if (!s || !job || !s->dirty || s->busy || !s->writable || s->state == HMI_NVM_CANCELLED ||
        (!s->immediate && (uint32_t)(now - s->last_change_ms) < s->debounce_ms &&
         (uint32_t)(now - s->first_dirty_ms) < s->max_delay_ms) ||
        (s->backoff && (uint32_t)(now - s->take_ms) < s->debounce_ms))
        return false;
    s->take_ms = now;
    memcpy(s->inflight, s->pending, s->pending_size);
    s->inflight_revision = s->ram_revision;
    s->busy = true;
    s->immediate = false;
    s->state = HMI_NVM_IN_PROGRESS;
    *job = (hmi_nvm_job_t){
        s->generation,
        s->inflight_revision,
        {s->type, s->product_namespace, s->schema, 0u, s->inflight_revision, s->inflight, s->pending_size}};
    return true;
}
bool hmi_nvm_complete(hmi_nvm_service_t *s, uint32_t generation, uint64_t revision,
                        hmi_slots_result_t result)
{
    if (!s || !s->busy || generation != s->generation || revision != s->inflight_revision)
        return false;
    s->busy = false;
    s->last_result = result;
    s->backoff = s->always_writable && result != HMI_SLOTS_OK;
    if (result == HMI_SLOTS_OK)
    {
        s->durable_revision = revision;
        s->dirty = s->ram_revision != revision;
        s->state = s->dirty ? HMI_NVM_DIRTY : HMI_NVM_DURABLE;
    }
    else
    {
        s->writable = s->always_writable;
        s->state = result == HMI_SLOTS_WRITE_INTERRUPTED ? HMI_NVM_UNCERTAIN : HMI_NVM_FAILED;
    }
    return true;
}
bool hmi_nvm_reconcile(hmi_nvm_service_t *s, hmi_slots_result_t result, const hmi_record_view_t *r)
{
    if (!s || s->busy || s->state == HMI_NVM_LOADING)
        return false;
    if (result == HMI_SLOTS_OK && !identity(s, r))
        result = HMI_SLOTS_INCOMPATIBLE;
    s->last_result = result;
    s->writable = s->always_writable || result == HMI_SLOTS_OK || result == HMI_SLOTS_EMPTY;
    if (!s->writable)
    {
        s->state = load_state(result);
        return false;
    }
    if (result == HMI_SLOTS_OK)
    {
        s->durable_revision = r->sequence;
        if (s->pending_size == r->payload_size && !memcmp(s->pending, r->payload, r->payload_size))
        {
            s->ram_revision = r->sequence;
            s->dirty = false;
            s->state = HMI_NVM_DURABLE;
            return true;
        }
        if (r->sequence >= s->ram_revision)
        {
            if (r->sequence == UINT64_MAX)
            {
                s->writable = false;
                s->state = HMI_NVM_FAILED;
                return false;
            }
            s->ram_revision = r->sequence + 1u;
        }
    }
    else
        s->durable_revision = 0u;
    s->dirty = true;
    s->immediate = true;
    s->state = HMI_NVM_DIRTY;
    return true;
}
bool hmi_nvm_cancel(hmi_nvm_service_t *s)
{
    if (!s || s->busy || !s->dirty || !s->writable)
        return false;
    s->state = HMI_NVM_CANCELLED;
    s->immediate = false;
    return true;
}
bool hmi_nvm_barrier(const hmi_nvm_service_t *s, uint64_t target)
{
    return s && target != 0u && s->durable_revision >= target && s->state != HMI_NVM_UNCERTAIN &&
           s->state != HMI_NVM_IO_ERROR;
}
const char *hmi_nvm_state_name(hmi_nvm_state_t state)
{
    static const char *const names[] = {"LOADING",     "EMPTY",        "READY",    "DIRTY",
                                        "IN_PROGRESS", "DURABLE",      "FAILED",   "UNCERTAIN",
                                        "CORRUPT",     "INCOMPATIBLE", "IO_ERROR", "CANCELLED_BEFORE_IO"};
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "INVALID";
}
