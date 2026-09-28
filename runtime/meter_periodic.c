#include "runtime/meter_periodic.h"
#include "runtime/meter_execution.h"
#include "contracts/meter_time.h"
#include <string.h>

static bool separate(const meter_tx_value_t *a, const meter_tx_value_t *b, size_t n)
{
    if (!n) return true;
    if (!a || !b || n > SIZE_MAX / sizeof(*a)) return false;
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    size_t bytes = n * sizeof(*a);
    return x < y ? y - x >= bytes : x - y >= bytes;
}
bool meter_tx_publish(meter_tx_publication_t *p, size_t capacity, const meter_tx_value_t *v,
                      size_t count, uint32_t generation, uint32_t now)
{
    if (!p || !generation || count > capacity || !separate(p->values, v, count)) return false;
    bool changed = !p->revision || p->count != count;
    for (size_t i = 0u; i < count; ++i)
    {
        if (v[i].valid && now - v[i].sample_ms >= METER_TIME_HALF_RANGE) return false;
        if (p->generation == generation && i < p->count && p->values[i].valid && v[i].valid &&
            v[i].sample_ms - p->values[i].sample_ms >= METER_TIME_HALF_RANGE) return false;
        if (i >= p->count || v[i].value != p->values[i].value || v[i].valid != p->values[i].valid) changed = true;
    }
    if (changed && p->revision == UINT64_MAX) return false;
    if (count) memcpy(p->values, v, count * sizeof(*v));
    if (changed) ++p->revision;
    p->generation = generation; p->published_ms = now; p->count = count;
    return true;
}
bool meter_tx_copy(meter_tx_publication_t *out, size_t capacity, const meter_tx_publication_t *in)
{
    if (!out || !in || in->count > capacity || !separate(out->values, in->values, in->count)) return false;
    meter_tx_value_t *values = out->values;
    if (in->count) memcpy(values, in->values, in->count * sizeof(*values));
    *out = *in; out->values = values;
    return true;
}
bool meter_periodic_valid(const meter_periodic_frame_t *p, size_t count)
{
    return p && p->period_ms && p->period_ms < METER_TIME_HALF_RANGE &&
        (unsigned)p->frame.bus < METER_BUS_COUNT && p->frame.size <= 8u &&
        p->max_age_ms < METER_TIME_HALF_RANGE && p->expiry_ms <= p->period_ms &&
        p->first_value <= count && p->value_count <= count - p->first_value &&
        (unsigned)p->freshness <= METER_TX_SUPPRESS && (unsigned)p->backlog <= METER_TX_REPLACE_PENDING &&
        (unsigned)p->commit <= METER_TX_COMMIT_DRIVER;
}
bool meter_periodic_reset(meter_periodic_state_t *s, const meter_periodic_frame_t *p, uint32_t generation, uint32_t now)
{
    if (!s || !p || !generation || !p->period_ms || p->period_ms >= METER_TIME_HALF_RANGE) return false;
    *s = (meter_periodic_state_t){.generation = generation, .wire = p->initial_wire};
    return meter_deadline_arm(&s->deadline, now, p->period_ms);
}
bool meter_periodic_prepare(meter_periodic_state_t *s, const meter_periodic_frame_t *p,
    const meter_tx_publication_t *v, bool allowed, uint32_t now, meter_periodic_message_t *out)
{
    if (!s || !p || !out || !p->period_ms) return false;
    uint32_t due = s->deadline.next_ms;
    if (!meter_deadline_take(&s->deadline, now)) return false;
    /* 长暂停只采用最近计划期限，不把遗漏帧补入队列。 */
    due += ((now - due) / p->period_ms) * p->period_ms;
    uint32_t expiry = due + (p->expiry_ms ? p->expiry_ms : p->period_ms);
    if (!allowed || s->busy || meter_time_reached(now, expiry) || s->ticket == UINT64_MAX)
    { ++s->skipped; return false; }
    bool fresh = true;
    if (p->encode)
    {
        if (!v || !v->revision || v->generation != s->generation || !meter_periodic_valid(p, v->count) ||
            (v->count && !v->values)) { ++s->rejected; return false; }
        for (size_t i = p->first_value; i < p->first_value + p->value_count; ++i)
        {
            uint32_t age = now - v->values[i].sample_ms;
            if (!v->values[i].valid || age >= METER_TIME_HALF_RANGE || (p->max_age_ms && age >= p->max_age_ms)) fresh = false;
        }
        if (!fresh) ++s->stale;
        if (!fresh && p->freshness == METER_TX_SUPPRESS) return false;
    }
    meter_can_frame_t frame = p->frame;
    uint32_t next_wire = s->wire;
    meter_tx_snapshot_t view = {0};
    if (v) view = (meter_tx_snapshot_t){.generation = v->generation, .published_ms = v->published_ms,
        .revision = v->revision, .count = v->count, .values = v->values};
    if (p->encode && !p->encode(&view, fresh, s->wire, &frame, &next_wire)) { ++s->rejected; return false; }
    if (frame.bus != p->frame.bus || frame.id != p->frame.id || frame.extended != p->frame.extended || frame.remote != p->frame.remote || frame.size > 8u)
    { ++s->rejected; return false; }
    s->proposed_wire = next_wire;
    *out = (meter_periodic_message_t){.generation = s->generation, .ticket = ++s->ticket,
        .revision = v ? v->revision : 0u, .published_ms = v ? v->published_ms : 0u,
        .deadline_ms = due, .expiry_ms = expiry, .queued_ms = now, .frame = frame};
    return true;
}
bool meter_periodic_admit(meter_periodic_state_t *s, const meter_periodic_frame_t *p, const meter_periodic_message_t *m)
{
    if (!s || !p || !m || s->busy || !m->ticket || m->generation != s->generation || m->ticket != s->ticket || m->ticket == s->pending_ticket) return false;
    s->busy = true; s->pending_ticket = m->ticket;
    if (p->commit == METER_TX_COMMIT_ADMISSION) s->wire = s->proposed_wire;
    return true;
}
bool meter_periodic_complete(meter_periodic_state_t *s, const meter_periodic_frame_t *p, const meter_periodic_result_t *r)
{
    if (!s || !p || !r || !s->busy || r->generation != s->generation || r->ticket != s->pending_ticket) return false;
    s->busy = false;
    if (r->success)
    { ++s->completed; if (p->commit == METER_TX_COMMIT_DRIVER) s->wire = s->proposed_wire; }
    else ++s->failed;
    return true;
}
bool meter_periodic_sendable(const meter_periodic_message_t *m, uint32_t generation, uint32_t now)
{
    return m && m->generation == generation && !meter_time_reached(now, m->expiry_ms);
}
