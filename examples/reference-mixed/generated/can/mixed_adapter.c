/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include "contracts/meter_product.h"
#include "protocols/common/meter_frame_router.h"
#include "catalog/catalog.h"
#include "mixed.h"
bool mixed_can0_decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    if (!meter_frame_valid(f) || !sink || f->bus != 0) return false;
    bool ok = true;
    switch (f->id)
    {
    case 256:
    {
        if (f->extended != false || f->size != 8) return false;
        struct mixed_motion_t raw;
        if (mixed_motion_unpack(&raw, f->data, f->size)) return false;
        {
            float v = mixed_motion_speed_decode(raw.speed);
            bool bad = v < 0.0f || v > 50.0f;
            meter_update_t u = {MIXED_CAN0_SPEED, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 31}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    case 257:
    {
        if (f->extended != false || f->size != 8) return false;
        struct mixed_energy_t raw;
        if (mixed_energy_unpack(&raw, f->data, f->size)) return false;
        {
            float v = mixed_energy_soc_decode(raw.soc);
            bool bad = v < 0.0f || v > 100.0f;
            meter_update_t u = {MIXED_CAN0_SOC, {v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, 31}};
            ok &= sink(ctx, &u);
        }
        break;
    }
    default: return false;
    }
    return ok;
}
