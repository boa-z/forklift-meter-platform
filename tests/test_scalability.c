#include "core/meter_core.h"
#include "core/meter_settings.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                          \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
/* 用一个假想的私有产品规模做替身，各项数量都超过平台旧有的上限。
 * 身份落在私有区间并按步长递增，因此身份不等于表内位置，
 * 这里不代表任何客户目录。 */
#define SIGNAL_COUNT 128
#define PARAMETER_COUNT 128
#define MONITOR_COUNT 96
#define FAULT_COUNT 192
#define STRIDE 4
#define ID(i) ((meter_signal_id_t)(METER_ID_PRIVATE_FIRST + (unsigned)(i) * STRIDE))
static const char synthetic_key[] = "synthetic";
static meter_signal_def_t signal_table[SIGNAL_COUNT];
static meter_parameter_def_t parameter_table[PARAMETER_COUNT];
static meter_monitor_def_t monitor_table[MONITOR_COUNT];
static meter_fault_def_t fault_table[FAULT_COUNT];
static const meter_catalog_t catalog = {
    signal_table, SIGNAL_COUNT, parameter_table, PARAMETER_COUNT, monitor_table, MONITOR_COUNT, fault_table,
    FAULT_COUNT};
static meter_value_t signal_slots[SIGNAL_COUNT];
static float parameter_slots[PARAMETER_COUNT];
static meter_fault_state_t fault_slots[FAULT_COUNT];
static meter_value_t other_signals[SIGNAL_COUNT];
static float other_parameters[PARAMETER_COUNT];
static meter_fault_state_t other_faults[FAULT_COUNT];
static uint8_t blob[METER_SETTINGS_OVERHEAD + PARAMETER_COUNT * 4u];
static void build(void)
{
    for (unsigned i = 0; i < SIGNAL_COUNT; ++i)
        signal_table[i] = (meter_signal_def_t){ID(i), synthetic_key, "u", 750};
    for (unsigned i = 0; i < PARAMETER_COUNT; ++i)
        parameter_table[i] = (meter_parameter_def_t){ID(i), synthetic_key, "u", 0.0f, 1000.0f, 25.0f};
    for (unsigned i = 0; i < MONITOR_COUNT; ++i)
        monitor_table[i] = (meter_monitor_def_t){synthetic_key, "u", ID(i)};
    for (unsigned i = 0; i < FAULT_COUNT; ++i)
        fault_table[i] = (meter_fault_def_t){ID(i), synthetic_key, synthetic_key};
}
static meter_core_storage_t storage_for(meter_value_t *signals, float *parameters,
                                                meter_fault_state_t *faults)
{
    return (meter_core_storage_t){signals, SIGNAL_COUNT, parameters, PARAMETER_COUNT, faults, FAULT_COUNT};
}
static int identities(void)
{
    build();
    meter_core_storage_t storage = storage_for(signal_slots, parameter_slots, fault_slots);
    meter_core_t core;
    CHECK(meter_core_init(&core, &catalog, &storage));
    CHECK(core.snapshot.catalog->signal_count == SIGNAL_COUNT);
    CHECK(core.snapshot.catalog->fault_count == FAULT_COUNT);
    for (unsigned i = 0; i < SIGNAL_COUNT; ++i)
    {
        meter_value_t value = meter_snapshot_read(&core.snapshot, ID(i));
        CHECK(meter_catalog_index(&catalog, ID(i)) == i);
        CHECK(value.state == METER_VALUE_UNKNOWN && value.value == 0 && value.timestamp_ms == 0);
    }
    /* 落在私有区间但超出表长的身份是未知数据，不是槽位。 */
    CHECK(meter_catalog_index(&catalog, ID(SIGNAL_COUNT)) == SIGNAL_COUNT);
    CHECK(meter_snapshot_read(&core.snapshot, ID(SIGNAL_COUNT)).state == METER_VALUE_UNKNOWN);
    /* 监控项不占域存储：它解析到自己所展示的那个信号。 */
    for (unsigned i = 0; i < MONITOR_COUNT; ++i)
        CHECK(meter_catalog_index(&catalog, monitor_table[i].signal) == i);
    /* 恰好等于目录规模的存储被接受，少一个槽位则在写入前就被拒绝。 */
    meter_core_storage_t short_one = storage;
    short_one.fault_capacity = FAULT_COUNT - 1;
    CHECK(!meter_core_init(&core, &catalog, &short_one));
    short_one = storage;
    short_one.signal_capacity = SIGNAL_COUNT - 1;
    CHECK(!meter_core_init(&core, &catalog, &short_one));
    /* 指向未声明信号的监控项会展示另一条数据的值，必须拒绝。 */
    monitor_table[MONITOR_COUNT - 1] = (meter_monitor_def_t){synthetic_key, "u", ID(SIGNAL_COUNT)};
    CHECK(!meter_core_init(&core, &catalog, &storage));
    monitor_table[MONITOR_COUNT - 1] = (meter_monitor_def_t){synthetic_key, "u", ID(MONITOR_COUNT - 1)};
    return 0;
}
static int updates(void)
{
    build();
    meter_core_storage_t storage = storage_for(signal_slots, parameter_slots, fault_slots);
    meter_core_t core;
    CHECK(meter_core_init(&core, &catalog, &storage));
    for (unsigned i = 0; i < SIGNAL_COUNT; ++i)
    {
        meter_update_t update = {ID(i), {(float)i + 0.5f, 1000u + i, METER_VALUE_VALID}};
        CHECK(meter_core_apply(&core, &update));
    }
    for (unsigned i = 0; i < SIGNAL_COUNT; ++i)
    {
        meter_value_t value = meter_snapshot_read(&core.snapshot, ID(i));
        CHECK(value.state == METER_VALUE_VALID);
        CHECK(value.value == (float)i + 0.5f);
        CHECK(value.timestamp_ms == 1000u + i);
    }
    /* 声明 128 个信号不能顺手把第 129 个槽位交给解码器。 */
    meter_update_t stray = {ID(SIGNAL_COUNT), {1.0f, 1, METER_VALUE_VALID}};
    CHECK(!meter_core_apply(&core, &stray));
    meter_core_tick(&core, 1000u + SIGNAL_COUNT + 750);
    unsigned stale = 0;
    for (unsigned i = 0; i < SIGNAL_COUNT; ++i)
        if (meter_snapshot_read(&core.snapshot, ID(i)).state == METER_VALUE_STALE)
            ++stale;
    CHECK(stale == SIGNAL_COUNT);
    return 0;
}
static int state(void)
{
    build();
    meter_core_storage_t storage = storage_for(signal_slots, parameter_slots, fault_slots);
    meter_core_storage_t copy = storage_for(other_signals, other_parameters, other_faults);
    meter_core_t core, other;
    CHECK(meter_core_init(&core, &catalog, &storage));
    CHECK(meter_core_init(&other, &catalog, &copy));
    float value = 0;
    for (unsigned i = 0; i < PARAMETER_COUNT; ++i)
        CHECK(meter_snapshot_parameter(&core.snapshot, ID(i), &value) && value == 25.0f);
    for (unsigned i = 0; i < PARAMETER_COUNT; ++i)
        CHECK(meter_core_parameter(&core, ID(i), (float)i));
    CHECK(!meter_core_parameter(&core, ID(0), 1001.0f));
    CHECK(!meter_core_parameter(&core, ID(PARAMETER_COUNT), 1.0f));
    CHECK(meter_snapshot_parameter(&core.snapshot, ID(PARAMETER_COUNT - 1), &value));
    CHECK(value == (float)PARAMETER_COUNT - 1);
    unsigned expected = 0;
    for (unsigned i = 0; i < FAULT_COUNT; ++i)
        if (i % 2)
        {
            CHECK(meter_snapshot_fault_set(&core.snapshot, ID(i), true));
            ++expected;
        }
    unsigned active = 0;
    for (unsigned i = 0; i < FAULT_COUNT; ++i)
    {
        CHECK(meter_snapshot_fault_active(&core.snapshot, ID(i)) == (i % 2 != 0));
        CHECK(core.snapshot.faults[i].id == ID(i));
        if (core.snapshot.faults[i].active)
            ++active;
    }
    CHECK(active == expected && expected == FAULT_COUNT / 2);
    CHECK(meter_snapshot_fault_set(&core.snapshot, ID(1), false));
    CHECK(!meter_snapshot_fault_active(&core.snapshot, ID(1)));
    CHECK(!meter_snapshot_fault_set(&core.snapshot, ID(FAULT_COUNT), true));
    /* 设置块的大小跟随产品表，平台无需为它预留容量。 */
    CHECK(meter_settings_size(&core) == sizeof(blob));
    CHECK(meter_settings_encode(&core, blob, sizeof(blob)));
    CHECK(meter_settings_decode(&other, blob, sizeof(blob)));
    CHECK(meter_snapshot_parameter(&other.snapshot, ID(PARAMETER_COUNT - 1), &value));
    CHECK(value == (float)PARAMETER_COUNT - 1);
    CHECK(meter_snapshot_parameter(&other.snapshot, ID(0), &value) && value == 0.0f);
    return 0;
}
int main(void)
{
    int result = identities();
    if (!result)
        result = updates();
    if (!result)
        result = state();
    printf("scalability: %s\n", result ? "FAIL" : "PASS");
    if (!result)
        puts("Synthetic 128 signals / 128 parameters / 96 monitors / 192 faults bound by the product PASS");
    return result;
}
