#include "core/meter_settings.h"
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
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
size_t meter_settings_size(const meter_core_t *core)
{
    if (!core || !core->snapshot.catalog || sizeof(float) != 4 ||
        core->snapshot.catalog->parameter_count > UINT16_MAX)
        return 0;
    return METER_SETTINGS_OVERHEAD + core->snapshot.catalog->parameter_count * METER_SETTINGS_ENTRY_SIZE;
}
bool meter_settings_encode(const meter_core_t *core, uint8_t *out, size_t size)
{
    const size_t length = meter_settings_size(core);
    if (!length || !out || size < length || (unsigned)core->snapshot.can_rate > METER_CAN_RATE_500K)
        return false;
    const meter_catalog_t *catalog = core->snapshot.catalog;
    memset(out, 0, length);
    memcpy(out, "MSP3", 4);
    out[4] = core->snapshot.imperial;
    out[5] = core->snapshot.brightness;
    out[6] = (uint8_t)core->snapshot.language;
    out[7] = (uint8_t)core->snapshot.can_rate;
    put32(out + 8, (uint32_t)catalog->parameter_count);
    for (size_t i = 0; i < catalog->parameter_count; ++i)
    {
        uint32_t bits;
        memcpy(&bits, &core->snapshot.parameters[i], 4);
        put32(out + 12 + i * METER_SETTINGS_ENTRY_SIZE, catalog->parameters[i].id);
        put32(out + 16 + i * METER_SETTINGS_ENTRY_SIZE, bits);
    }
    /* 内层校验只检测缓冲损坏；介质完整性由 FMP2 的 CRC 与 seal 负责。 */
    put32(out + length - 4, checksum(out, length - 4));
    return true;
}
bool meter_settings_decode(meter_core_t *core, const uint8_t *data, size_t size)
{
    if (!core || !data || size != meter_settings_size(core) || size < METER_SETTINGS_OVERHEAD ||
        memcmp(data, "MSP3", 4) || data[7] > METER_CAN_RATE_500K ||
        data[4] > 1 || data[5] < 10 || data[5] > 100 ||
        data[6] > METER_LANGUAGE_ZH || get32(data + size - 4) != checksum(data, size - 4))
        return false;
    const meter_can_rate_t rate = (meter_can_rate_t)data[7];
    const meter_catalog_t *catalog = core->snapshot.catalog;
    if (get32(data + 8) != catalog->parameter_count)
        return false;
    for (size_t i = 0; i < catalog->parameter_count; ++i)
    {
        const uint8_t *entry = data + 12 + i * METER_SETTINGS_ENTRY_SIZE;
        const uint32_t id = get32(entry);
        if (id > UINT16_MAX || !meter_core_parameter_valid(core, (uint16_t)id, param32(entry + 4)))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (get32(data + 12 + j * METER_SETTINGS_ENTRY_SIZE) == id)
                return false;
    }
    bool changed = core->snapshot.imperial != (data[4] != 0) || core->snapshot.brightness != data[5] ||
                   core->snapshot.language != (meter_language_t)data[6] || core->snapshot.can_rate != rate;
    /* 全部校验成功后才按稳定 ID 应用，目录调整顺序不改变参数身份。 */
    for (size_t i = 0; i < catalog->parameter_count; ++i)
    {
        const uint8_t *entry = data + 12 + i * METER_SETTINGS_ENTRY_SIZE;
        const size_t index = meter_catalog_parameter_index(catalog, (uint16_t)get32(entry));
        const float value = param32(entry + 4);
        changed = changed || core->snapshot.parameters[index] != value;
        core->snapshot.parameters[index] = value;
    }
    core->snapshot.imperial = data[4] != 0;
    core->snapshot.brightness = data[5];
    core->snapshot.language = (meter_language_t)data[6];
    core->snapshot.can_rate = rate;
    if (changed)
        ++core->snapshot.revision;
    return true;
}
