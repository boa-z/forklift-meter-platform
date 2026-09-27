#include "core/meter_core.h"
#include <math.h>
#include <string.h>
bool meter_core_init(meter_core_t *core, const meter_catalog_t *catalog)
{
    if (!core || !catalog || catalog->parameter_count > METER_PARAMETER_CAPACITY ||
        catalog->monitor_count > METER_MONITOR_CAPACITY || catalog->fault_count > METER_FAULT_CAPACITY)
        return false;
    if ((catalog->parameter_count && !catalog->parameters) ||
        (catalog->monitor_count && !catalog->monitors) || (catalog->fault_count && !catalog->faults))
        return false;
    memset(core, 0, sizeof(*core));
    core->catalog = catalog;
    core->snapshot.brightness = 80;
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        core->snapshot.parameters[i] = catalog->parameters[i].initial;
    return true;
}
bool meter_core_apply(void *context, const meter_update_t *update)
{
    meter_core_t *core = context;
    if (!core || !update || (unsigned)update->signal >= METER_SIGNAL_COUNT ||
        (unsigned)update->value.state > METER_VALUE_ERROR)
        return false;
    meter_value_t value = update->value;
    if (!isfinite(value.value))
    {
        value.value = 0;
        value.state = METER_VALUE_ERROR;
    }
    core->snapshot.signals[update->signal] = value;
    return true;
}
void meter_core_tick(meter_core_t *core, uint32_t now_ms, uint32_t stale_ms)
{
    for (size_t i = 0; i < METER_SIGNAL_COUNT; ++i)
    {
        meter_value_t *v = &core->snapshot.signals[i];
        if (v->state == METER_VALUE_VALID && (uint32_t)(now_ms - v->timestamp_ms) >= stale_ms)
            v->state = METER_VALUE_STALE;
    }
}
void meter_core_connection(meter_core_t *core, bool connected, uint32_t generation)
{
    core->snapshot.connected = connected;
    core->snapshot.generation = generation;
    if (!connected)
        for (size_t i = 0; i < METER_SIGNAL_COUNT; ++i)
            if (core->snapshot.signals[i].state == METER_VALUE_VALID)
                core->snapshot.signals[i].state = METER_VALUE_STALE;
}
bool meter_core_parameter(meter_core_t *core, uint16_t id, float value)
{
    if (!isfinite(value))
        return false;
    for (size_t i = 0; i < core->catalog->parameter_count; ++i)
    {
        const meter_parameter_def_t *p = &core->catalog->parameters[i];
        if (p->id == id)
        {
            if (value < p->min || value > p->max)
                return false;
            core->snapshot.parameters[i] = value;
            return true;
        }
    }
    return false;
}
bool meter_core_action(meter_core_t *core, const meter_action_t *action)
{
    if (!action || !isfinite(action->value))
        return false;
    switch (action->kind)
    {
    case METER_ACTION_UNITS:
        if (action->value != 0 && action->value != 1)
            return false;
        core->snapshot.imperial = action->value != 0;
        return true;
    case METER_ACTION_BRIGHTNESS:
        if (action->value < 10 || action->value > 100)
            return false;
        core->snapshot.brightness = (uint8_t)action->value;
        return true;
    case METER_ACTION_PARAMETER:
        return meter_core_parameter(core, action->id, action->value);
    default:
        return false;
    }
}
const meter_snapshot_t *meter_core_snapshot(const meter_core_t *core)
{
    return &core->snapshot;
}
