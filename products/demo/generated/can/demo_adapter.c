/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include "contracts/meter_product.h"
#include "protocols/common/meter_frame_router.h"
#include "generated/demo_catalog.h"
#include "demo.h"
bool meter_demo_decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    if (!meter_frame_valid(f) || !sink || f->bus != 0) return false;
    bool ok = true;
    switch (f->id)
    {
    case 256:
    {
        if (f->extended != false || f->size != 8) return false;
        struct demo_motion_t raw;
        if (demo_motion_unpack(&raw, f->data, f->size)) return false;
        {
            float v = demo_motion_speed_decode(raw.speed);
            bool bad = v < 0.0f || v > 50.0f;
            meter_update_t u = {METER_SPEED, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_motion_steering_decode(raw.steering);
            bool bad = v < -45.0f || v > 45.0f;
            meter_update_t u = {METER_STEERING, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_motion_hours_decode(raw.hours);
            bool bad = v < 0.0f || v > 6553.5f;
            meter_update_t u = {METER_WORK_HOURS, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 257:
    {
        if (f->extended != false || f->size != 8) return false;
        struct demo_energy_t raw;
        if (demo_energy_unpack(&raw, f->data, f->size)) return false;
        {
            float v = demo_energy_soc_decode(raw.soc);
            bool bad = v < 0.0f || v > 100.0f;
            meter_update_t u = {METER_SOC, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_energy_voltage_decode(raw.voltage);
            bool bad = v < 0.0f || v > 100.0f;
            meter_update_t u = {METER_BATTERY_VOLTAGE, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_energy_charging_decode(raw.charging);
            bool bad = v < 0.0f || v > 1.0f;
            meter_update_t u = {METER_CHARGING, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 258:
    {
        if (f->extended != false || f->size != 8) return false;
        struct demo_lift_t raw;
        if (demo_lift_unpack(&raw, f->data, f->size)) return false;
        {
            float v = demo_lift_height_decode(raw.height);
            bool bad = v < 0.0f || v > 6.0f;
            meter_update_t u = {METER_HEIGHT, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 259:
    {
        if (f->extended != false || f->size != 8) return false;
        struct demo_load_t raw;
        if (demo_load_unpack(&raw, f->data, f->size)) return false;
        {
            float v = demo_load_weight_decode(raw.weight);
            bool bad = v < 0.0f || v > 1500.0f;
            meter_update_t u = {METER_LOAD, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 260:
    {
        if (f->extended != false || f->size != 8) return false;
        struct demo_status_t raw;
        if (demo_status_unpack(&raw, f->data, f->size)) return false;
        {
            float v = demo_status_seat_decode(raw.seat);
            bool bad = v < 0.0f || v > 1.0f;
            meter_update_t u = {METER_SEAT, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_status_brake_decode(raw.brake);
            bool bad = v < 0.0f || v > 1.0f;
            meter_update_t u = {METER_BRAKE, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_status_neutral_decode(raw.neutral);
            bool bad = v < 0.0f || v > 1.0f;
            meter_update_t u = {METER_NEUTRAL, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_status_warning_decode(raw.warning);
            bool bad = v < 0.0f || v > 1.0f;
            meter_update_t u = {METER_WARNING, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_status_motor_temp_decode(raw.motor_temp);
            bool bad = v < -40.0f || v > 150.0f;
            meter_update_t u = {METER_MOTOR_TEMP, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        {
            float v = demo_status_controller_temp_decode(raw.controller_temp);
            bool bad = v < -40.0f || v > 150.0f;
            meter_update_t u = {METER_CONTROLLER_TEMP, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 1}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    default: return false;
    }
    return ok;
}
