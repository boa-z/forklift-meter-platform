#include "core/meter_core.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
/* 身份 0 表示“无此条目”，因此半成品目录无法占用槽位；两张表若共用同一身份，
 * 后写入的一方会静默覆盖前者的数据，必须在绑定前拒绝。 */
static bool ids_valid(const void *table, size_t count, size_t stride, size_t offset)
{
    for (size_t i = 0; i < count; ++i)
    {
        const uint16_t *id = (const uint16_t *)((const char *)table + i * stride + offset);
        if (!*id)
            return false;
        for (size_t j = i + 1; j < count; ++j)
            if (*id == *(const uint16_t *)((const char *)table + j * stride + offset))
                return false;
    }
    return true;
}
static bool catalog_valid(const meter_catalog_t *catalog)
{
    if (!catalog || !catalog->signal_count || !catalog->signals)
        return false;
    if ((catalog->parameter_count && !catalog->parameters) ||
        (catalog->monitor_count && !catalog->monitors) || (catalog->fault_count && !catalog->faults))
        return false;
    if (!ids_valid(catalog->signals, catalog->signal_count, sizeof(*catalog->signals),
                   offsetof(meter_signal_def_t, id)) ||
        !ids_valid(catalog->parameters, catalog->parameter_count, sizeof(*catalog->parameters),
                   offsetof(meter_parameter_def_t, id)) ||
        !ids_valid(catalog->faults, catalog->fault_count, sizeof(*catalog->faults),
                   offsetof(meter_fault_def_t, id)))
        return false;
    /* 监视项只是指向已声明信号的呈现映射；引用未声明的身份会让这一行显示别人的值。 */
    for (size_t i = 0; i < catalog->monitor_count; ++i)
        if (meter_catalog_index(catalog, catalog->monitors[i].signal) >= catalog->signal_count)
            return false;
    for (size_t i = 0; i < catalog->parameter_count; ++i)
    {
        const meter_parameter_def_t *p = &catalog->parameters[i];
        if (!isfinite(p->min) || !isfinite(p->max) || !isfinite(p->initial) || p->min > p->max ||
            p->initial < p->min || p->initial > p->max)
            return false;
    }
    return true;
}
bool meter_core_init(meter_core_t *core, const meter_catalog_t *catalog, const meter_core_storage_t *storage)
{
    if (!core || !storage || !catalog_valid(catalog) || catalog->signal_count > storage->signal_capacity ||
        catalog->parameter_count > storage->parameter_capacity ||
        catalog->fault_count > storage->fault_capacity || !storage->signals ||
        (catalog->parameter_count && !storage->parameters) || (catalog->fault_count && !storage->faults))
        return false;
    memset(core, 0, sizeof(*core));
    core->storage = *storage;
    core->snapshot.catalog = catalog;
    core->snapshot.signals = storage->signals;
    core->snapshot.parameters = storage->parameters;
    core->snapshot.faults = storage->faults;
    for (size_t i = 0; i < catalog->signal_count; ++i)
        core->snapshot.signals[i] = meter_value_unknown();
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        core->snapshot.parameters[i] = catalog->parameters[i].initial;
    for (size_t i = 0; i < catalog->fault_count; ++i)
        core->snapshot.faults[i] = (meter_fault_state_t){catalog->faults[i].id, false};
    core->snapshot.brightness = 80;
    return true;
}
bool meter_core_apply(void *context, const meter_update_t *update)
{
    meter_core_t *core = context;
    if (!core || !update || (unsigned)update->value.state > METER_VALUE_ERROR)
        return false;
    size_t index = meter_catalog_index(core->snapshot.catalog, update->signal);
    if (index >= core->snapshot.catalog->signal_count)
        return false;
    meter_value_t value = update->value;
    if (!isfinite(value.value))
    {
        value.value = 0;
        value.state = METER_VALUE_ERROR;
    }
    const meter_value_t current = core->snapshot.signals[index];
    const meter_source_policy_fn_t policy = core->snapshot.catalog->source_policy;
    if (policy && !policy(core->snapshot.catalog->source_policy_context, update->signal, value.source,
                          current.source))
        return false;
    core->snapshot.signals[index] = value;
    ++core->snapshot.revision;
    return true;
}
void meter_core_tick(meter_core_t *core, uint32_t now_ms, uint32_t stale_ms)
{
    const meter_catalog_t *catalog = core->snapshot.catalog;
    for (size_t i = 0; i < catalog->signal_count; ++i)
    {
        meter_value_t *v = &core->snapshot.signals[i];
        const uint32_t limit = catalog->signals[i].stale_ms ? catalog->signals[i].stale_ms : stale_ms;
        if (v->state == METER_VALUE_VALID && limit && (uint32_t)(now_ms - v->timestamp_ms) >= limit)
            v->state = METER_VALUE_STALE;
    }
}
void meter_core_connection(meter_core_t *core, bool connected, uint32_t generation)
{
    core->snapshot.connected = connected;
    core->snapshot.generation = generation;
    if (connected)
        return;
    const meter_catalog_t *catalog = core->snapshot.catalog;
    for (size_t i = 0; i < catalog->signal_count; ++i)
        if (core->snapshot.signals[i].state == METER_VALUE_VALID)
            core->snapshot.signals[i].state = METER_VALUE_STALE;
}
bool meter_core_parameter_valid(const meter_core_t *core, uint16_t id, float value)
{
    if (!core || !isfinite(value))
        return false;
    size_t index = meter_catalog_parameter_index(core->snapshot.catalog, id);
    if (index >= core->snapshot.catalog->parameter_count)
        return false;
    const meter_parameter_def_t *p = &core->snapshot.catalog->parameters[index];
    return value >= p->min && value <= p->max;
}
bool meter_core_parameter(meter_core_t *core, uint16_t id, float value)
{
    if (!meter_core_parameter_valid(core, id, value))
        return false;
    core->snapshot.parameters[meter_catalog_parameter_index(core->snapshot.catalog, id)] = value;
    return true;
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
        ++core->snapshot.revision;
        return true;
    case METER_ACTION_BRIGHTNESS:
        if (action->value < 10 || action->value > 100)
            return false;
        core->snapshot.brightness = (uint8_t)action->value;
        ++core->snapshot.revision;
        return true;
    case METER_ACTION_PARAMETER:
        return meter_core_parameter(core, action->id, action->value);
    case METER_ACTION_LANGUAGE:
        if (action->value != METER_LANGUAGE_EN && action->value != METER_LANGUAGE_ZH)
            return false;
        core->snapshot.language = (meter_language_t)action->value;
        ++core->snapshot.revision;
        return true;
    default:
        return false;
    }
}
const meter_snapshot_t *meter_core_snapshot(const meter_core_t *core)
{
    return core ? &core->snapshot : NULL;
}
