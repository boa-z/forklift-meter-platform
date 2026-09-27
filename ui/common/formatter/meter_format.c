#include "ui/common/formatter/meter_format.h"
#include <math.h>
#include <stdio.h>
bool meter_gauge_map(float value, float min, float max, float start, float end, float *angle)
{
    if (!angle || !isfinite(value) || !isfinite(min) || !isfinite(max) || !isfinite(start) ||
        !isfinite(end) || min >= max)
        return false;
    if (value < min)
        value = min;
    if (value > max)
        value = max;
    *angle = start + (end - start) * (value - min) / (max - min);
    return true;
}
unsigned meter_threshold_band(float value, float warning, float low)
{
    return value < warning ? 0u : value < low ? 1u : 2u;
}
void meter_format_value(char *out, size_t size, float value, meter_value_state_t state, const char *unit,
                        unsigned decimals)
{
    if (state == METER_VALUE_UNKNOWN)
        snprintf(out, size, "-- %s", unit);
    else if (state == METER_VALUE_ERROR || !isfinite(value))
        snprintf(out, size, "ERR %s", unit);
    else
        snprintf(out, size, "%.*f %s%s", (int)(decimals > 2 ? 2 : decimals), (double)value, unit,
                 state == METER_VALUE_STALE ? " / STALE" : "");
}
