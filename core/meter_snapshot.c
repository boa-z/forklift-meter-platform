#include "core/meter_snapshot.h"
#include <stdint.h>
#include <string.h>

/* 地址区间只用于拒绝别名；不从整数构造或解引用指针。 */
typedef struct
{
    uintptr_t start;
    size_t size;
} memory_span_t;

static bool span_make(memory_span_t *span, const void *pointer, size_t count, size_t element)
{
    if ((count > SIZE_MAX / element) || ((count != 0u) && (pointer == NULL)))
    {
        return false;
    }
    span->start = (uintptr_t)pointer;
    span->size = count * element;
    return span->size <= UINTPTR_MAX - span->start;
}

static bool overlap(memory_span_t first, memory_span_t second)
{
    return (first.size != 0u) && (second.size != 0u) &&
           (first.start < second.start + second.size) && (second.start < first.start + first.size);
}

meter_snapshot_result_t meter_snapshot_copy(meter_snapshot_t *destination,
                                           const meter_core_storage_t *storage,
                                           const meter_snapshot_t *source)
{
    if ((destination == NULL) || (storage == NULL) || (source == NULL) || (source->catalog == NULL))
    {
        return METER_SNAPSHOT_INVALID;
    }
    const meter_catalog_t *catalog = source->catalog;
    if ((storage->signal_capacity < catalog->signal_count) ||
        (storage->parameter_capacity < catalog->parameter_count) ||
        (storage->fault_capacity < catalog->fault_count))
    {
        return METER_SNAPSHOT_CAPACITY;
    }
    memory_span_t writes[4];
    memory_span_t reads[10];
    if (!span_make(&writes[0], destination, 1u, sizeof(*destination)) ||
        !span_make(&writes[1], storage->signals, catalog->signal_count, sizeof(*storage->signals)) ||
        !span_make(&writes[2], storage->parameters, catalog->parameter_count, sizeof(*storage->parameters)) ||
        !span_make(&writes[3], storage->faults, catalog->fault_count, sizeof(*storage->faults)) ||
        !span_make(&reads[0], source, 1u, sizeof(*source)) ||
        !span_make(&reads[1], source->signals, catalog->signal_count, sizeof(*source->signals)) ||
        !span_make(&reads[2], source->parameters, catalog->parameter_count, sizeof(*source->parameters)) ||
        !span_make(&reads[3], source->faults, catalog->fault_count, sizeof(*source->faults)) ||
        !span_make(&reads[4], storage, 1u, sizeof(*storage)) ||
        !span_make(&reads[5], catalog, 1u, sizeof(*catalog)) ||
        !span_make(&reads[6], catalog->signals, catalog->signal_count, sizeof(*catalog->signals)) ||
        !span_make(&reads[7], catalog->parameters, catalog->parameter_count, sizeof(*catalog->parameters)) ||
        !span_make(&reads[8], catalog->faults, catalog->fault_count, sizeof(*catalog->faults)) ||
        !span_make(&reads[9], catalog->monitors, catalog->monitor_count, sizeof(*catalog->monitors)))
    {
        return METER_SNAPSHOT_INVALID;
    }
    for (size_t i = 0u; i < 4u; ++i)
    {
        for (size_t j = 0u; j < 10u; ++j)
        {
            if (overlap(writes[i], reads[j]))
            {
                return METER_SNAPSHOT_ALIAS;
            }
        }
        for (size_t j = 0u; j < i; ++j)
        {
            if (overlap(writes[i], writes[j]))
            {
                return METER_SNAPSHOT_ALIAS;
            }
        }
    }
    if (catalog->signal_count != 0u)
    {
        memcpy(storage->signals, source->signals, writes[1].size);
    }
    if (catalog->parameter_count != 0u)
    {
        memcpy(storage->parameters, source->parameters, writes[2].size);
    }
    if (catalog->fault_count != 0u)
    {
        memcpy(storage->faults, source->faults, writes[3].size);
    }
    meter_snapshot_t copy = *source;
    copy.signals = storage->signals;
    copy.parameters = storage->parameters;
    copy.faults = storage->faults;
    *destination = copy;
    return METER_SNAPSHOT_COPIED;
}
