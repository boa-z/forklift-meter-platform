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
static float param32(const uint8_t *p)
{
    uint32_t bits = get32(p);
    float v;
    memcpy(&v, &bits, 4);
    return v;
}
static size_t body(const meter_catalog_t *catalog)
{
    return catalog->parameter_count * 4u;
}
size_t meter_settings_size(const meter_core_t *core)
{
    if (!core || core->snapshot.catalog->parameter_count > 255u)
        return 0;
    return METER_SETTINGS_OVERHEAD + body(core->snapshot.catalog);
}
bool meter_settings_encode(const meter_core_t *core, uint8_t *out, size_t size)
{
    size_t length = meter_settings_size(core);
    if (sizeof(float) != 4 || !length || !out || size < length)
        return false;
    const meter_catalog_t *catalog = core->snapshot.catalog;
    memset(out, 0, size);
    memcpy(out, "FMP1", 4);
    out[4] = core->snapshot.imperial;
    out[5] = core->snapshot.brightness;
    out[6] = (uint8_t)catalog->parameter_count;
    out[7] = (uint8_t)core->snapshot.language;
    for (size_t i = 0; i < catalog->parameter_count; ++i)
    {
        uint32_t bits;
        memcpy(&bits, &core->snapshot.parameters[i], 4);
        put32(out + 8 + i * 4, bits);
    }
    put32(out + length - 4, checksum(out, length - 4));
    return true;
}
bool meter_settings_decode(meter_core_t *core, const uint8_t *data, size_t size)
{
    if (!core || !data || size < METER_SETTINGS_OVERHEAD || memcmp(data, "FMP1", 4))
        return false;
    /* 文件内记录的参数个数决定合法长度，因此更大的目录读更大的文件，平台不知道任何容量。 */
    if (size != METER_SETTINGS_OVERHEAD + (size_t)data[6] * 4u)
        return false;
    const meter_catalog_t *catalog = core->snapshot.catalog;
    if (data[6] != catalog->parameter_count || data[4] > 1 || data[5] < 10 || data[5] > 100 ||
        data[7] > METER_LANGUAGE_ZH || get32(data + size - 4) != checksum(data, size - 4))
        return false;
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        if (!meter_core_parameter_valid(core, catalog->parameters[i].id, param32(data + 8 + i * 4)))
            return false;
    /* 以上只校验不写入，因此被拒绝的文件不会让 core 停留在半应用状态。 */
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        core->snapshot.parameters[i] = param32(data + 8 + i * 4);
    core->snapshot.imperial = data[4] != 0;
    core->snapshot.language = (meter_language_t)data[7];
    core->snapshot.brightness = data[5];
    return true;
}
