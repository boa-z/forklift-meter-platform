#include "runtime/meter_periodic.h"
#include "contracts/meter_time.h"
#include <stddef.h>

bool meter_deadline_arm(meter_deadline_t *d, uint32_t now, uint32_t period)
{
    if ((d == NULL) || (period == 0u) || (period >= METER_TIME_HALF_RANGE))
        return false;
    *d = (meter_deadline_t){.period_ms = period, .next_ms = now + period, .armed = true};
    return true;
}
bool meter_deadline_take(meter_deadline_t *d, uint32_t now)
{
    if ((d == NULL) || !d->armed || (d->period_ms == 0u) || !meter_time_reached(now, d->next_ms))
        return false;
    uint32_t late = now - d->next_ms;
    uint32_t skipped = late / d->period_ms;
    if (late > d->late_ms)
        d->late_ms = late;
    d->missed = skipped > UINT32_MAX - d->missed ? UINT32_MAX : d->missed + skipped;
    d->next_ms += (skipped + 1u) * d->period_ms;
    return true;
}
bool meter_periodic_valid(const meter_periodic_frame_t *p)
{
    return p && (unsigned)p->frame.bus < METER_BUS_COUNT && p->frame.size <= 8u && p->period_ms &&
           p->period_ms < METER_TIME_HALF_RANGE &&
           p->frame.id <= (p->frame.extended ? 0x1fffffffu : 0x7ffu);
}
bool meter_periodic_due(meter_deadline_t *deadline, const meter_periodic_frame_t *definition,
                        const meter_snapshot_t *snapshot, uint32_t now, bool allowed, bool in_flight,
                        meter_can_frame_t *frame)
{
    if (!deadline || !definition || !frame)
        return false;
    if (!deadline->armed)
    {
        /* 第一帧在启用后立即发出，之后按周期。 */
        if (!meter_deadline_arm(deadline, now, definition->period_ms))
            return false;
        deadline->next_ms = now;
    }
    if (!meter_deadline_take(deadline, now) || !allowed || in_flight)
        return false;
    *frame = definition->frame;
    frame->timestamp_ms = now;
    return !definition->encode || definition->encode(snapshot, now, frame);
}
