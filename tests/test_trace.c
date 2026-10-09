#include "diagnostics/meter_trace.h"
#include <stdio.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                       \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static meter_trace_t ring;
static meter_trace_entry_t copy[METER_TRACE_CAPACITY];
int main(void)
{
    _Static_assert(sizeof(meter_trace_entry_t) == 16, "trace entry size");
    meter_trace_init(&ring);
    CHECK(ring.count == 0 && ring.overwritten == 0);
    CHECK(meter_trace_snapshot(&ring, copy, 256) == 0);
    for (unsigned i = 0; i < 300; ++i)
        meter_trace_append(&ring, UINT32_MAX - 100 + i, METER_TRACE_SDO, SDO_START, i, 2 * i);
    CHECK(ring.count == 256 && ring.write_index == 44 && ring.overwritten == 44);
    CHECK(meter_trace_snapshot(&ring, copy, 256) == 256);
    for (unsigned i = 0; i < 256; ++i)
    {
        CHECK(copy[i].arg0 == 44 + i && copy[i].arg1 == 2 * (44 + i));
        CHECK(copy[i].timestamp_ms == (uint32_t)(UINT32_MAX - 100 + 44 + i));
    }
    CHECK(meter_trace_snapshot(&ring, copy, 3) == 3 && copy[0].arg0 == 297 && copy[2].arg0 == 299);
    meter_trace_clear(&ring);
    CHECK(ring.count == 0 && ring.overwritten == 0 && ring.write_index == 0 && ring.sequence == 300);
    meter_trace_append(&ring, 5, METER_TRACE_RUNTIME, RUNTIME_CONNECT, 0, 0);
    CHECK(meter_trace_snapshot(&ring, copy, 256) == 1 && copy[0].timestamp_ms == 5);
    CHECK(meter_trace_snapshot(NULL, copy, 1) == 0 && meter_trace_snapshot(&ring, NULL, 1) == 0);
    return 0;
}
