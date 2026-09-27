#include "sim/synthetic.h"
#include <math.h>
static void put16(uint8_t *p, int n)
{
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)((unsigned)n >> 8);
}
static float triangle(uint32_t now, uint32_t period)
{
    float t = (now % period) / (float)period;
    return t < 0.5f ? t * 2 : (1 - t) * 2;
}
static void feed(meter_runtime_t *r, uint32_t now, float speed, float soc, float steer, float height,
                 float load, bool warning, bool error, bool charge)
{
    meter_can_frame_t f = {METER_BUS_CAN0, 0x100, now, false, false, 8, {0}};
    put16(f.data, (int)(speed * 100));
    put16(f.data + 2, (int)(steer * 100));
    put16(f.data + 4, 12460 + (int)(now / 360000));
    meter_runtime_push(r, &f);
    f.id = 0x101;
    for (unsigned i = 0; i < 8; ++i)
        f.data[i] = 0;
    f.data[0] = (uint8_t)soc;
    put16(f.data + 1, 4800);
    f.data[3] = charge;
    meter_runtime_push(r, &f);
    f.id = 0x102;
    for (unsigned i = 0; i < 8; ++i)
        f.data[i] = 0;
    put16(f.data, error ? 65535 : (int)(height * 1000));
    meter_runtime_push(r, &f);
    f.id = 0x103;
    for (unsigned i = 0; i < 8; ++i)
        f.data[i] = 0;
    put16(f.data, (int)load);
    meter_runtime_push(r, &f);
    f.id = 0x104;
    for (unsigned i = 0; i < 8; ++i)
        f.data[i] = 0;
    f.data[0] = (uint8_t)(1 | (speed < 0.5f ? 6 : 0) | (warning ? 8 : 0));
    put16(f.data + 1, warning ? 92 : 52);
    put16(f.data + 3, 43);
    meter_runtime_push(r, &f);
}
void meter_synthetic_values(meter_runtime_t *r, uint32_t now, float speed, float soc, float steer)
{
    feed(r, now, speed, soc, steer, 3.25f, 850, false, false, false);
}
void meter_synthetic_step(meter_runtime_t *r, uint32_t now, demo_scenario_t scenario)
{
    if (scenario == DEMO_OFFLINE)
    {
        meter_runtime_connection(r, false);
        return;
    }
    meter_runtime_connection(r, true);
    if (scenario == DEMO_STALE || scenario == DEMO_UNKNOWN)
        return;
    float t = triangle(now, 40000), soc = 100 - 90 * triangle(now, 60000);
    feed(r, now, 50 * t, scenario == DEMO_WARNING ? 10 : soc, -45 + 90 * triangle(now, 16000),
         6 * triangle(now, 24000), 1200 * triangle(now, 32000), scenario == DEMO_WARNING,
         scenario == DEMO_ERROR, (now % 60000) > 30000);
}
