#include "products/demo/product.h"
#include <math.h>
static bool above(const meter_snapshot_t *s, meter_signal_id_t id, float value)
{
    return s->signals[id].state == METER_VALUE_VALID && s->signals[id].value > value;
}
void meter_demo_evaluate(meter_snapshot_t *s)
{
    uint32_t f = 0;
    if (s->signals[METER_SOC].state == METER_VALUE_VALID && s->signals[METER_SOC].value < s->parameters[3])
        f |= 1u << 0;
    if (above(s, METER_MOTOR_TEMP, s->parameters[5]))
        f |= 1u << 1;
    if (s->signals[METER_BATTERY_VOLTAGE].state == METER_VALUE_VALID &&
        s->signals[METER_BATTERY_VOLTAGE].value < 40)
        f |= 1u << 2;
    for (size_t i = 0; i < METER_SIGNAL_COUNT; ++i)
    {
        if (s->signals[i].state == METER_VALUE_ERROR)
            f |= 1u << 3;
        if (s->signals[i].state == METER_VALUE_STALE)
            f |= 1u << 4;
    }
    if (!s->connected)
        f |= 1u << 4;
    if (above(s, METER_LOAD, s->parameters[4]))
        f |= 1u << 5;
    if (above(s, METER_HEIGHT, s->parameters[2]))
        f |= 1u << 6;
    if (above(s, METER_CONTROLLER_TEMP, s->parameters[6]))
        f |= 1u << 7;
    if (above(s, METER_WARNING, 0))
        f |= 1u << 8;
    if (s->signals[METER_STEERING].state == METER_VALUE_VALID &&
        fabsf(s->signals[METER_STEERING].value) > s->parameters[9])
        f |= 1u << 9;
    s->active_faults = f;
}
