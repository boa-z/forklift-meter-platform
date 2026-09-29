#include "core/meter_core.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
void meter_core_bind_diagnostics(meter_core_t *core, meter_diagnostics_t *d)
{
    if (core)
    {
        core->diag = d;
        if (d)
            d->domain = &core->snapshot;
    }
}
static void diag_stale(meter_core_t *core, meter_signal_id_t id, uint32_t now)
{
    METER_DIAG_INC(core->diag, domain, stale_transition);
    METER_DIAG_TRACE(core->diag, METER_TRACE_DOMAIN, DOMAIN_STALE, now, id, 0);
}
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
static bool value_equal(const meter_value_t *a, const meter_value_t *b)
{
    return a->value == b->value && a->timestamp_ms == b->timestamp_ms && a->state == b->state &&
           a->source == b->source;
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
    core->snapshot.can_rate = METER_CAN_RATE_500K;
    return true;
}
bool meter_core_init_with_settings(meter_core_t *core, const meter_catalog_t *catalog,
                                  const meter_core_storage_t *storage,
                                  const meter_initial_settings_t *settings)
{
    if (settings && ((unsigned)settings->language > METER_LANGUAGE_ZH ||
                     (unsigned)settings->can_rate > METER_CAN_RATE_500K ||
                     settings->brightness < 10u || settings->brightness > 100u))
        return false;
    if (!meter_core_init(core, catalog, storage))
        return false;
    if (settings)
    {
        core->snapshot.language = settings->language;
        core->snapshot.can_rate = settings->can_rate;
        core->snapshot.brightness = settings->brightness;
        core->snapshot.imperial = settings->imperial;
    }
    return true;
}
static bool apply_value(meter_core_t *core, const meter_update_t *update, bool arbitrate)
{
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
    if (arbitrate && policy && !policy(core->snapshot.catalog->source_policy_context, update->signal, &value, &current))
        return false;
    METER_DIAG_INC(core->diag, domain, updates);
    if (value.state == METER_VALUE_ERROR)
        METER_DIAG_INC(core->diag, domain, error_updates);
    if (value.state == METER_VALUE_VALID && current.state != METER_VALUE_VALID)
    {
        METER_DIAG_INC(core->diag, domain, valid_transition);
        METER_DIAG_TRACE(core->diag, METER_TRACE_DOMAIN, DOMAIN_RECOVER, value.timestamp_ms, update->signal,
                         value.source);
    }
    if (value.state == METER_VALUE_ERROR && current.state != METER_VALUE_ERROR)
        METER_DIAG_TRACE(core->diag, METER_TRACE_DOMAIN, DOMAIN_ERROR, value.timestamp_ms, update->signal,
                         value.source);
    if (current.source != METER_SOURCE_NONE && value.source != current.source)
    {
        METER_DIAG_INC(core->diag, domain, source_switch);
        METER_DIAG_TRACE(core->diag, METER_TRACE_DOMAIN, DOMAIN_SOURCE_SWITCH, value.timestamp_ms,
                         update->signal, ((uint32_t)current.source << 16) | value.source);
    }
    if (value.state == METER_VALUE_STALE && current.state != METER_VALUE_STALE)
        diag_stale(core, update->signal, value.timestamp_ms);
    if (value_equal(&value, &current))
        return true;
    core->snapshot.signals[index] = value;
    ++core->snapshot.revision;
    return true;
}
bool meter_core_apply(void *context, const meter_update_t *update)
{
    return apply_value(context, update, true);
}
bool meter_core_apply_batch(meter_core_t *core, const meter_update_batch_t *batch)
{
    if (!core || !batch || !batch->updates || !batch->count || !batch->sequence ||
        !batch->source || batch->generation != core->snapshot.generation) return false;
    const meter_catalog_t *c = core->snapshot.catalog;
    for (size_t i = 0u; i < batch->count; ++i)
    {
        const meter_update_t *u = &batch->updates[i];
        size_t index = meter_catalog_index(c, u->signal);
        if (index >= c->signal_count || u->value.source != batch->source ||
            (unsigned)u->value.state > (unsigned)METER_VALUE_ERROR) return false;
        for (size_t j = 0u; j < i; ++j)
            if (batch->updates[j].signal == u->signal) return false;
        meter_value_t value = u->value;
        if (!isfinite(value.value)) { value.value = 0.0f; value.state = METER_VALUE_ERROR; }
        if (c->source_policy && !c->source_policy(c->source_policy_context, u->signal,
                                                 &value, &core->snapshot.signals[index])) return false;
    }
    for (size_t i = 0u; i < batch->count; ++i)
        (void)apply_value(core, &batch->updates[i], false);
    return true;
}
void meter_core_tick(meter_core_t *core, uint32_t now_ms)
{
    const meter_catalog_t *catalog = core->snapshot.catalog;
    for (size_t i = 0; i < catalog->signal_count; ++i)
    {
        meter_value_t *v = &core->snapshot.signals[i];
        const uint32_t limit = catalog->signals[i].stale_ms;
        if (v->state == METER_VALUE_VALID && limit && (uint32_t)(now_ms - v->timestamp_ms) >= limit)
        {
            v->state = METER_VALUE_STALE;
            diag_stale(core, catalog->signals[i].id, now_ms);
            ++core->snapshot.revision;
        }
    }
}
void meter_core_connection(meter_core_t *core, bool connected, uint32_t generation)
{
    bool changed = core->snapshot.connected != connected || core->snapshot.generation != generation;
    core->snapshot.connected = connected;
    core->snapshot.generation = generation;
    if (connected)
    {
        if (changed)
            ++core->snapshot.revision;
        return;
    }
    const meter_catalog_t *catalog = core->snapshot.catalog;
    for (size_t i = 0; i < catalog->signal_count; ++i)
        if (core->snapshot.signals[i].state == METER_VALUE_VALID)
        {
            core->snapshot.signals[i].state = METER_VALUE_STALE;
            diag_stale(core, catalog->signals[i].id, core->diag ? core->diag->data.uptime_ms : 0);
            changed = true;
        }
    if (changed)
        ++core->snapshot.revision;
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
    size_t index = meter_catalog_parameter_index(core->snapshot.catalog, id);
    if (core->snapshot.parameters[index] == value)
        return true;
    core->snapshot.parameters[index] = value;
    ++core->snapshot.revision;
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
        bool next_units = action->value != 0;
        if (core->snapshot.imperial == next_units)
            return true;
        core->snapshot.imperial = next_units;
        ++core->snapshot.revision;
        return true;
    case METER_ACTION_BRIGHTNESS:
        if (action->value < 10 || action->value > 100)
            return false;
        uint8_t next_brightness = (uint8_t)action->value;
        if (core->snapshot.brightness == next_brightness)
            return true;
        core->snapshot.brightness = next_brightness;
        ++core->snapshot.revision;
        return true;
    case METER_ACTION_PARAMETER:
        return meter_core_parameter(core, action->id, action->value);
    case METER_ACTION_LANGUAGE:
        if (action->value != METER_LANGUAGE_EN && action->value != METER_LANGUAGE_ZH)
            return false;
        meter_language_t next_language = (meter_language_t)action->value;
        if (core->snapshot.language == next_language)
            return true;
        core->snapshot.language = next_language;
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

bool meter_core_profile(meter_core_t *core, bool confirmed, uint16_t family, uint64_t capabilities)
{
    if (!core || (confirmed && family == 0u) || (!confirmed && (family != 0u || capabilities != 0u)))
        return false;
    const meter_profile_t current = core->snapshot.profile;
    if (current.confirmed == confirmed && current.family == family && current.capabilities == capabilities)
        return true;
    if (current.generation == UINT32_MAX)
        return false;
    const meter_profile_t next = {current.generation + 1u, family, capabilities, confirmed};
    core->snapshot.profile = next;
    ++core->snapshot.revision;
    return true;
}
