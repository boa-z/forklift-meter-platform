#include "diagnostics/meter_trace.h"
#include <string.h>
void meter_trace_init(meter_trace_t *t)
{
    if (t)
        memset(t, 0, sizeof(*t));
}
void meter_trace_append(meter_trace_t *t, uint32_t now, uint16_t module, uint16_t event, uint32_t a,
                        uint32_t b)
{
    if (!t)
        return;
    t->entries[t->write_index] = (meter_trace_entry_t){now, module, event, a, b};
    t->write_index = (t->write_index + 1) % METER_TRACE_CAPACITY;
    if (t->count < METER_TRACE_CAPACITY)
        ++t->count;
    else if (t->overwritten < UINT32_MAX)
        ++t->overwritten;
    ++t->sequence;
}
size_t meter_trace_snapshot(const meter_trace_t *t, meter_trace_entry_t *out, size_t capacity)
{
    if (!t || !out)
        return 0;
    size_t count = t->count < capacity ? t->count : capacity;
    size_t begin = (t->write_index + METER_TRACE_CAPACITY - count) % METER_TRACE_CAPACITY;
    for (size_t i = 0; i < count; ++i)
        out[i] = t->entries[(begin + i) % METER_TRACE_CAPACITY];
    return count;
}
void meter_trace_clear(meter_trace_t *t)
{
    if (t)
    {
        t->count = 0;
        t->write_index = 0;
        t->overwritten = 0;
    }
}
