#include "platform/common/hmi_display_ccm.h"
#include <stdbool.h>

int32_t hmi_display_ccm_normalize(unsigned index, long value)
{
    const bool offset = ((index == 3u) || (index == 7u) || (index == 11u));
    const unsigned bits = offset ? 9u : 12u;
    const long mask = (1L << bits) - 1L;
    long field = value & mask;
    if (field > (mask >> 1))
    {
        field -= (mask + 1L);
    }
    return (int32_t)field;
}
