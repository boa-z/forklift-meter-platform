#include "services/settings_app.h"
#include "generated/demo_catalog.h"
#include "product/product.h"
#include <math.h>
#include <stddef.h>
static bool above(const meter_snapshot_t *s, meter_signal_id_t id, float limit)
{
    meter_value_t value = meter_snapshot_read(s, id);
    return value.state == METER_VALUE_VALID && value.value > limit;
}
static bool below(const meter_snapshot_t *s, meter_signal_id_t id, float limit)
{
    meter_value_t value = meter_snapshot_read(s, id);
    return value.state == METER_VALUE_VALID && value.value < limit;
}
static bool any_state(const meter_snapshot_t *s, meter_value_state_t state)
{
    for (size_t i = 0; i < s->catalog->signal_count; ++i)
        if (s->signals[i].state == state)
            return true;
    return false;
}
static bool threshold(const meter_snapshot_t *s, uint16_t parameter_id, float *out)
{
    return meter_snapshot_parameter(s, parameter_id, out);
}
void meter_demo_evaluate(meter_snapshot_t *s)
{
    demo_settings_publish(s);
    float limit = 0;
    meter_snapshot_fault_set(s, DEMO_FAULT_LOW_CHARGE,
                             threshold(s, DEMO_PARAMETER_BATTERY_WARNING_LEVEL, &limit) &&
                                 below(s, METER_SOC, limit));
    meter_snapshot_fault_set(s, DEMO_FAULT_OVERTEMPERATURE,
                             threshold(s, DEMO_PARAMETER_MOTOR_TEMP_ADVISORY, &limit) &&
                                 above(s, METER_MOTOR_TEMP, limit));
    meter_snapshot_fault_set(s, DEMO_FAULT_LOW_VOLTAGE, below(s, METER_BATTERY_VOLTAGE, 40));
    meter_snapshot_fault_set(s, DEMO_FAULT_SENSOR_FAULT, any_state(s, METER_VALUE_ERROR));
    meter_snapshot_fault_set(s, DEMO_FAULT_COMMUNICATION_FAULT,
                             any_state(s, METER_VALUE_STALE) || !s->connected);
    meter_snapshot_fault_set(s, DEMO_FAULT_LOAD_ADVISORY,
                             threshold(s, DEMO_PARAMETER_LOAD_ADVISORY, &limit) &&
                                 above(s, METER_LOAD, limit));
    meter_snapshot_fault_set(s, DEMO_FAULT_HEIGHT_ADVISORY,
                             threshold(s, DEMO_PARAMETER_LIFT_LIMIT, &limit) &&
                                 above(s, METER_HEIGHT, limit));
    meter_snapshot_fault_set(s, DEMO_FAULT_CONTROLLER_HOT,
                             threshold(s, DEMO_PARAMETER_CONTROLLER_TEMP_ADVISORY, &limit) &&
                                 above(s, METER_CONTROLLER_TEMP, limit));
    meter_snapshot_fault_set(s, DEMO_FAULT_GENERIC_WARNING, above(s, METER_WARNING, 0));
    meter_value_t steering = meter_snapshot_read(s, METER_STEERING);
    float degree = 0;
    meter_snapshot_fault_set(s, DEMO_FAULT_STEERING_ADVISORY,
                             steering.state == METER_VALUE_VALID &&
                                 threshold(s, DEMO_PARAMETER_STEERING_LIMIT, &degree) &&
                                 fabsf(steering.value) > degree);
}
