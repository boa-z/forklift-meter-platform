#include "runtime/meter_execution.h"
#include "contracts/meter_time.h"
#include <stddef.h>
void meter_execution_init(meter_execution_t *e)
{
    if (e != NULL)
        *e = (meter_execution_t){.state = METER_EXEC_INIT, .mode = METER_MODE_STARTUP, .generation = 1u};
}
bool meter_execution_transition(meter_execution_t *e, meter_execution_state_t next)
{
    if ((e == NULL) || ((unsigned)next > (unsigned)METER_EXEC_FAILED))
        return false;
    bool allowed = false;
    switch (e->state)
    {
    case METER_EXEC_INIT: allowed = next == METER_EXEC_READY || next == METER_EXEC_FAILED; break;
    case METER_EXEC_READY: allowed = next == METER_EXEC_RUNNING || next == METER_EXEC_FAILED || next == METER_EXEC_STOPPING; break;
    case METER_EXEC_RUNNING: allowed = next == METER_EXEC_STOPPING || next == METER_EXEC_FAILED; break;
    case METER_EXEC_FAILED: allowed = next == METER_EXEC_STOPPING; break;
    case METER_EXEC_STOPPING: allowed = next == METER_EXEC_STOPPED; break;
    case METER_EXEC_STOPPED: break;
    default: break;
    }
    if (allowed) e->state = next;
    return allowed;
}
bool meter_execution_mode(meter_execution_t *e, meter_mode_t mode)
{
    if ((e == NULL) || ((unsigned)mode > (unsigned)METER_MODE_SHUTDOWN) ||
        ((e->state != METER_EXEC_RUNNING) && (mode != METER_MODE_SHUTDOWN)))
        return false;
    if (e->mode == mode) return true;
    if (e->generation == UINT32_MAX) return false;
    ++e->generation;
    e->mode = mode;
    return true;
}
meter_mode_policy_t meter_execution_policy(meter_mode_t mode)
{
    meter_mode_policy_t p = {0};
    switch (mode)
    {
    case METER_MODE_NORMAL: p = (meter_mode_policy_t){true, true, true, true, true, true}; break;
    case METER_MODE_DEGRADED: p.telemetry = true; p.critical_tx = true; p.normal_ui = true; break;
    case METER_MODE_UPDATE_MAINTENANCE: p.critical_tx = true; break;
    case METER_MODE_STARTUP: break;
    case METER_MODE_SHUTDOWN: break;
    default: break;
    }
    return p;
}
bool meter_deadline_arm(meter_deadline_t *d, uint32_t now, uint32_t period)
{
    if ((d == NULL) || (period == 0u) || (period >= METER_TIME_HALF_RANGE)) return false;
    *d = (meter_deadline_t){.period_ms = period, .next_ms = now + period, .armed = true};
    return true;
}
bool meter_deadline_take(meter_deadline_t *d, uint32_t now)
{
    if ((d == NULL) || !d->armed || (d->period_ms == 0u) || !meter_time_reached(now, d->next_ms))
        return false;
    uint32_t late = now - d->next_ms;
    uint32_t skipped = late / d->period_ms;
    if (late > d->late_ms) d->late_ms = late;
    d->missed = skipped > UINT32_MAX - d->missed ? UINT32_MAX : d->missed + skipped;
    d->next_ms += (skipped + 1u) * d->period_ms;
    return true;
}
