#include "diagnostics/hmi_trace.h"
#include <stdio.h>
#include "check.h"
static hmi_trace_t ring;
static hmi_trace_entry_t copy[HMI_TRACE_CAPACITY];
int main(void)
{
    _Static_assert(sizeof(hmi_trace_entry_t) == 16, "trace entry size");
    hmi_trace_init(&ring);
    CHECK(ring.count == 0 && ring.overwritten == 0);
    CHECK(hmi_trace_snapshot(&ring, copy, 256) == 0);
    for (unsigned i = 0; i < 300; ++i)
        hmi_trace_append(&ring, UINT32_MAX - 100 + i, HMI_TRACE_CAN, CAN_RX_DROP, i, 2 * i);
    CHECK(ring.count == 256 && ring.write_index == 44 && ring.overwritten == 44);
    CHECK(hmi_trace_snapshot(&ring, copy, 256) == 256);
    for (unsigned i = 0; i < 256; ++i)
    {
        CHECK(copy[i].arg0 == 44 + i && copy[i].arg1 == 2 * (44 + i));
        CHECK(copy[i].timestamp_ms == (uint32_t)(UINT32_MAX - 100 + 44 + i));
    }
    CHECK(hmi_trace_snapshot(&ring, copy, 3) == 3 && copy[0].arg0 == 297 && copy[2].arg0 == 299);
    hmi_trace_clear(&ring);
    CHECK(ring.count == 0 && ring.overwritten == 0 && ring.write_index == 0 && ring.sequence == 300);
    hmi_trace_append(&ring, 5, HMI_TRACE_RUNTIME, RUNTIME_CONNECT, 0, 0);
    CHECK(hmi_trace_snapshot(&ring, copy, 256) == 1 && copy[0].timestamp_ms == 5);
    CHECK(hmi_trace_snapshot(NULL, copy, 1) == 0 && hmi_trace_snapshot(&ring, NULL, 1) == 0);
    return 0;
}
