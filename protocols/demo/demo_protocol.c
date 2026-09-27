#include "protocols/demo/demo_protocol.h"
#include "protocols/common/meter_frame_router.h"
static unsigned u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static int s16(const uint8_t *p)
{
    unsigned v = u16(p);
    return v >= 32768u ? (int)v - 65536 : (int)v;
}
static bool emit(meter_update_sink_t sink, void *context, const meter_can_frame_t *f, meter_signal_id_t id,
                 float value, bool bad)
{
    meter_update_t u = {id, {value, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID}};
    return sink(context, &u);
}
bool meter_demo_decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    if (!meter_frame_valid(f) || !sink || f->extended || f->bus != METER_BUS_CAN0 || f->size != 8)
        return false;
    bool ok = true;
    switch (f->id)
    {
    case 0x100:
        ok &= emit(sink, ctx, f, METER_SPEED, u16(f->data) / 100.0f, u16(f->data) > 5000);
        ok &= emit(sink, ctx, f, METER_STEERING, s16(f->data + 2) / 100.0f,
                   s16(f->data + 2) < -4500 || s16(f->data + 2) > 4500);
        ok &= emit(sink, ctx, f, METER_WORK_HOURS, u16(f->data + 4) / 10.0f, false);
        break;
    case 0x101:
        ok &= emit(sink, ctx, f, METER_SOC, f->data[0], f->data[0] > 100);
        ok &= emit(sink, ctx, f, METER_BATTERY_VOLTAGE, u16(f->data + 1) / 100.0f, u16(f->data + 1) > 10000);
        ok &= emit(sink, ctx, f, METER_CHARGING, (f->data[3] & 1) != 0, false);
        break;
    case 0x102:
        ok &= emit(sink, ctx, f, METER_HEIGHT, u16(f->data) / 1000.0f, u16(f->data) > 6000);
        break;
    case 0x103:
        ok &= emit(sink, ctx, f, METER_LOAD, (float)u16(f->data), u16(f->data) > 1500);
        break;
    case 0x104:
        ok &= emit(sink, ctx, f, METER_SEAT, (f->data[0] & 1) != 0, false);
        ok &= emit(sink, ctx, f, METER_BRAKE, (f->data[0] & 2) != 0, false);
        ok &= emit(sink, ctx, f, METER_NEUTRAL, (f->data[0] & 4) != 0, false);
        ok &= emit(sink, ctx, f, METER_WARNING, (f->data[0] & 8) != 0, false);
        ok &= emit(sink, ctx, f, METER_MOTOR_TEMP, (float)s16(f->data + 1),
                   s16(f->data + 1) > 150 || s16(f->data + 1) < -40);
        ok &= emit(sink, ctx, f, METER_CONTROLLER_TEMP, (float)s16(f->data + 3),
                   s16(f->data + 3) > 150 || s16(f->data + 3) < -40);
        break;
    default:
        return false;
    }
    return ok;
}
