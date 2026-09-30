#include "core/meter_settings.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static const meter_signal_def_t signals[] = {{.id = 1u}};
static const meter_parameter_def_t old_parameters[] = {
    {.id = 20u, .min = 0, .max = 100, .initial = 2},
    {.id = 40u, .min = 0, .max = 100, .initial = 4}};
static const meter_parameter_def_t new_parameters[] = {
    {.id = 40u, .min = 0, .max = 100, .initial = 4},
    {.id = 10u, .min = 0, .max = 100, .initial = 7},
    {.id = 20u, .min = 0, .max = 100, .initial = 2}};
static const meter_catalog_t old_catalog = {.signals = signals, .signal_count = 1u,
    .parameters = old_parameters, .parameter_count = 2u};
static const meter_catalog_t new_catalog = {.signals = signals, .signal_count = 1u,
    .parameters = new_parameters, .parameter_count = 3u};

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static void seal(uint8_t *p, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size - 4; ++i) hash = (hash ^ p[i]) * 16777619u;
    put32(p + size - 4, hash);
}
static void rejected(meter_core_t *core, const uint8_t *data, size_t size)
{
    const meter_snapshot_t before = core->snapshot;
    float parameters[3];
    memcpy(parameters, core->snapshot.parameters, sizeof(parameters));
    assert(!meter_settings_decode_append_only(core, data, size));
    assert(memcmp(&before, &core->snapshot, sizeof(before)) == 0);
    assert(memcmp(parameters, core->snapshot.parameters, sizeof(parameters)) == 0);
}
int main(void)
{
    meter_core_t old_core, new_core;
    meter_value_t old_signals[1], new_signals[1];
    float old_values[2], new_values[3];
    meter_core_storage_t old_storage = {.signals = old_signals, .signal_capacity = 1u,
        .parameters = old_values, .parameter_capacity = 2u};
    meter_core_storage_t new_storage = {.signals = new_signals, .signal_capacity = 1u,
        .parameters = new_values, .parameter_capacity = 3u};
    assert(meter_core_init(&old_core, &old_catalog, &old_storage));
    assert(meter_core_init(&new_core, &new_catalog, &new_storage));
    old_values[0] = 42; old_values[1] = 63; new_values[1] = 99;
    old_core.snapshot.language = METER_LANGUAGE_ZH;
    old_core.snapshot.brightness = 73;
    old_core.snapshot.can_rate = METER_CAN_RATE_125K;
    old_core.snapshot.imperial = true;
    uint8_t data[64], bad[64], expanded[64];
    const size_t size = meter_settings_size(&old_core);
    assert(meter_settings_encode(&old_core, data, sizeof(data)));
    assert(!meter_settings_decode(&new_core, data, size));
    assert(new_values[1] == 99 && new_values[0] == 4 && new_values[2] == 2);
    assert(meter_settings_decode_append_only(&new_core, data, size));
    assert(new_values[0] == 63 && new_values[1] == 7 && new_values[2] == 42);
    assert(new_core.snapshot.language == METER_LANGUAGE_ZH);
    assert(new_core.snapshot.brightness == 73 && new_core.snapshot.imperial);
    assert(new_core.snapshot.can_rate == METER_CAN_RATE_125K);
    uint32_t revision = new_core.snapshot.revision;
    assert(meter_settings_decode_append_only(&new_core, data, size));
    assert(new_core.snapshot.revision == revision);
    assert(meter_settings_encode(&new_core, expanded, sizeof(expanded)));
    assert(meter_settings_decode(&new_core, expanded, meter_settings_size(&new_core)));
    assert(new_core.snapshot.revision == revision);
    /* 关闭兼容仍拒绝旧长度；开启兼容也不容忍未知、重复、非法数值和损坏。 */
    for (size_t length = 0; length < size; ++length) rejected(&new_core, data, length);
    memcpy(bad, data, size); put32(bad + 12, 99u); seal(bad, size); rejected(&new_core, bad, size);
    memcpy(bad, data, size); put32(bad + 20, 20u); seal(bad, size); rejected(&new_core, bad, size);
    memcpy(bad, data, size); put32(bad + 8, 3u); seal(bad, size); rejected(&new_core, bad, size);
    const float invalid[] = {-1, 101, INFINITY, NAN};
    for (unsigned i = 0; i < 4u; ++i)
    {
        uint32_t bits; memcpy(&bits, &invalid[i], sizeof(bits));
        memcpy(bad, data, size); put32(bad + 16, bits); seal(bad, size); rejected(&new_core, bad, size);
    }
    memcpy(bad, data, size); bad[5] ^= 1; rejected(&new_core, bad, size);
    assert(!meter_settings_decode_append_only(NULL, data, size));
    assert(!meter_settings_decode_append_only(&new_core, NULL, size));
    return 0;
}
