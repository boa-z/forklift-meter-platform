#include "diagnostics/hmi_trace.h"
#include <string.h>
void hmi_trace_init(hmi_trace_t *t)
{
    if (t)
        memset(t, 0, sizeof(*t));
}
void hmi_trace_append(hmi_trace_t *t, uint32_t now, uint16_t module, uint16_t event, uint32_t a,
                        uint32_t b)
{
    if (!t)
        return;
    t->entries[t->write_index] = (hmi_trace_entry_t){now, module, event, a, b};
    t->write_index = (t->write_index + 1) % HMI_TRACE_CAPACITY;
    if (t->count < HMI_TRACE_CAPACITY)
        ++t->count;
    else if (t->overwritten < UINT32_MAX)
        ++t->overwritten;
    ++t->sequence;
}
size_t hmi_trace_snapshot(const hmi_trace_t *t, hmi_trace_entry_t *out, size_t capacity)
{
    if (!t || !out)
        return 0;
    size_t count = t->count < capacity ? t->count : capacity;
    size_t begin = (t->write_index + HMI_TRACE_CAPACITY - count) % HMI_TRACE_CAPACITY;
    for (size_t i = 0; i < count; ++i)
        out[i] = t->entries[(begin + i) % HMI_TRACE_CAPACITY];
    return count;
}
void hmi_trace_clear(hmi_trace_t *t)
{
    if (t)
    {
        t->count = 0;
        t->write_index = 0;
        t->overwritten = 0;
    }
}
