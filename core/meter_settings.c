#include "core/meter_settings.h"
#include <math.h>
#include <string.h>
static uint32_t checksum(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ p[i]) * 16777619u;
    return h;
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
bool meter_settings_encode(const meter_core_t *core, uint8_t out[METER_SETTINGS_SIZE])
{
    if (sizeof(float) != 4)
        return false;
    memset(out, 0, METER_SETTINGS_SIZE);
    memcpy(out, "FMP1", 4);
    out[4] = core->snapshot.imperial;
    out[5] = core->snapshot.brightness;
    out[6] = (uint8_t)core->catalog->parameter_count;
    out[7] = (uint8_t)core->snapshot.language;
    for (size_t i = 0; i < core->catalog->parameter_count; ++i)
    {
        uint32_t bits;
        memcpy(&bits, &core->snapshot.parameters[i], 4);
        put32(out + 8 + i * 4, bits);
    }
    put32(out + METER_SETTINGS_SIZE - 4, checksum(out, METER_SETTINGS_SIZE - 4));
    return true;
}
bool meter_settings_decode(meter_core_t *core, const uint8_t *data, size_t size)
{
    if (size != METER_SETTINGS_SIZE || memcmp(data, "FMP1", 4) || data[4] > 1 || data[5] < 10 ||
        data[5] > 100 || data[6] != core->catalog->parameter_count || data[7] > METER_LANGUAGE_ZH ||
        get32(data + size - 4) != checksum(data, size - 4))
        return false;
    meter_core_t candidate = *core;
    for (size_t i = 0; i < core->catalog->parameter_count; ++i)
    {
        float v;
        uint32_t bits = get32(data + 8 + i * 4);
        memcpy(&v, &bits, 4);
        if (!meter_core_parameter(&candidate, core->catalog->parameters[i].id, v))
            return false;
    }
    candidate.snapshot.imperial = data[4] != 0;
    candidate.snapshot.language = (meter_language_t)data[7];
    candidate.snapshot.brightness = data[5];
    *core = candidate;
    return true;
}
