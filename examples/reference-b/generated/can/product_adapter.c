/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include "contracts/meter_product.h"
#include "protocols/common/meter_frame_router.h"
#include "catalog/catalog.h"
#include "product.h"
bool product_decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    if (!meter_frame_valid(f) || !sink || f->bus != 1) return false;
    bool ok = true;
    switch (f->id)
    {
    case 419385857:
    {
        if (f->extended != true || f->size != 8) return false;
        struct product_drivetrain_t raw;
        if (product_drivetrain_unpack(&raw, f->data, f->size)) return false;
        {
            float v = product_drivetrain_velocity_decode(raw.velocity);
            bool bad = v < 0.0f || v > 12.0f;
            meter_update_t u = {PRODUCT_SPEED, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 19}};
            ok &= sink(ctx, &u);
        }
        {
            float v = product_drivetrain_torque_decode(raw.torque);
            bool bad = v < -100.0f || v > 100.0f;
            meter_update_t u = {REF_TORQUE, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 19}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 419385858:
    {
        if (f->extended != true || f->size != 8) return false;
        struct product_power_t raw;
        if (product_power_unpack(&raw, f->data, f->size)) return false;
        {
            float v = product_power_remaining_decode(raw.remaining);
            bool bad = v < 0.0f || v > 100.0f;
            meter_update_t u = {REF_SOC, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 19}};
            ok &= sink(ctx, &u);
        }
        {
            float v = product_power_ambient_decode(raw.ambient);
            bool bad = v < -40.0f || v > 120.0f;
            meter_update_t u = {REF_AMBIENT, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 19}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    default: return false;
    }
    return ok;
}
