#include "core/meter_core.h"
#include "core/meter_settings.h"
#include "generated/demo_catalog.h"
#include "protocol/demo_protocol.h"
#include "protocols/common/meter_frame_router.h"
#include "runtime/meter_runtime.h"
#include "sim/synthetic.h"
#include "ui/common/formatter/meter_format.h"
#include <math.h>
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
#define SLOTS(array) (sizeof(array) / sizeof((array)[0]))
/* 平台从不持有域内存：每个调用方绑定的数组按自己产品目录的大小准备。 */
static meter_value_t signal_slots[DEMO_SIGNAL_SLOTS];
static float parameter_slots[DEMO_PARAMETER_SLOTS];
static meter_fault_state_t fault_slots[DEMO_FAULT_SLOTS];
static const meter_signal_def_t duplicate_signals[] = {{METER_SPEED, "speed"}, {METER_SPEED, "flow"}};
static const meter_signal_def_t anonymous_signals[] = {{0, "gap"}};
static const meter_monitor_def_t dangling_monitor[] = {{"Gap", "gap", METER_WARNING + 100}};
static const meter_fault_def_t duplicate_faults[] = {{DEMO_FAULT_LOW_CHARGE, "one", "first"},
                                                     {DEMO_FAULT_LOW_CHARGE, "two", "second"}};
static bool keep_first_source(void *context, meter_signal_id_t signal, const meter_value_t *incoming,
                              const meter_value_t *current)
{
    (void)context;
    (void)signal;
    return current->source == METER_SOURCE_NONE || incoming->source == current->source;
}
static meter_core_storage_t storage(void)
{
    return (meter_core_storage_t){signal_slots,           SLOTS(signal_slots), parameter_slots,
                                  SLOTS(parameter_slots), fault_slots,         SLOTS(fault_slots)};
}
/* 设置块是公开文档化的格式，所以测试可以直接伪造一个合法文件，
 * 而不是只破坏字节直到校验和报错。 */
static void store_float(uint8_t *p, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, 4);
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(bits >> (8 * i));
}
static void seal(uint8_t *bytes, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i + 4 < size; ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    for (unsigned i = 0; i < 4; ++i)
        bytes[size - 4 + i] = (uint8_t)(hash >> (8 * i));
}
static const meter_frame_route_t routes[] = {{METER_BUS_CAN0, 0x100, false, 1},
                                             {METER_BUS_CAN1, 0x100, false, 2},
                                             {METER_BUS_CAN0, 0x100, true, 2},
                                             {METER_BUS_CAN1, 0x1abc123, true, 2}};
static const meter_route_profile_t route_profile = {routes, 4};
static bool noop(const meter_can_frame_t *f, meter_update_sink_t sink, void *context)
{
    (void)f;
    (void)sink;
    (void)context;
    return true;
}
static const meter_protocol_binding_t bindings[] = {{1, meter_demo_decode}, {2, noop}};
static const meter_protocol_profile_t protocol_profile = {bindings, 2};
static const meter_product_t product = {
    .id = "test", .protocols = &protocol_profile, .routes = &route_profile, .catalog = &meter_demo_catalog};
static meter_can_frame_t frame(void)
{
    meter_can_frame_t f = {METER_BUS_CAN0, 0x100, 1, false, false, 8, {0xc4, 0x09, 0, 0, 0, 0, 0, 0}};
    return f;
}
static int contracts(void)
{
    CHECK(meter_routes_valid(&route_profile));
    meter_can_frame_t f = frame();
    CHECK(meter_route_lookup(&route_profile, &f)->owner == 1);
    f.bus = METER_BUS_CAN1;
    CHECK(meter_route_lookup(&route_profile, &f)->owner == 2);
    f.bus = METER_BUS_CAN0;
    f.extended = true;
    CHECK(meter_route_lookup(&route_profile, &f)->owner == 2);
    f.bus = METER_BUS_CAN1;
    f.id = 0x1abc123;
    CHECK(meter_route_lookup(&route_profile, &f)->owner == 2);
    f.extended = false;
    CHECK(!meter_frame_valid(&f));
    f = frame();
    f.size = 9;
    CHECK(!meter_frame_valid(&f));
    f = frame();
    f.remote = true;
    CHECK(!meter_frame_valid(&f));
    f = frame();
    f.bus = (meter_bus_role_t)99;
    CHECK(!meter_frame_valid(&f));
    meter_frame_route_t duplicate[2] = {routes[0], routes[0]};
    meter_route_profile_t bad = {duplicate, 2};
    CHECK(!meter_routes_valid(&bad));
    return 0;
}
static int core(void)
{
    meter_core_t c;
    meter_core_storage_t bound = storage();
    CHECK(meter_core_init(&c, &meter_demo_catalog, &bound));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).state == METER_VALUE_UNKNOWN);
    CHECK(c.snapshot.revision == 0);
    meter_update_t u = {METER_SPEED, {12.5f, 100, METER_VALUE_VALID}};
    CHECK(meter_core_apply(&c, &u));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).source == METER_SOURCE_NONE);
    CHECK(c.snapshot.revision == 1);
    CHECK(meter_core_apply(&c, &u));
    CHECK(c.snapshot.revision == 1);
    u.signal = METER_SOC;
    u.value.timestamp_ms = 100;
    CHECK(meter_core_apply(&c, &u));
    u.signal = METER_SPEED;
    meter_core_tick(&c, 849);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).state == METER_VALUE_VALID);
    meter_core_tick(&c, 850);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).state == METER_VALUE_STALE);
    CHECK(c.snapshot.revision == 3);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).value == 12.5f);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SOC).state == METER_VALUE_VALID);
    meter_catalog_t policy_catalog = meter_demo_catalog;
    policy_catalog.source_policy = keep_first_source;
    CHECK(meter_core_init(&c, &policy_catalog, &bound));
    u = (meter_update_t){METER_SPEED, {1, 0, METER_VALUE_VALID, 7}};
    CHECK(meter_core_apply(&c, &u));
    u.value.source = 8;
    CHECK(!meter_core_apply(&c, &u));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).source == 7);
    CHECK(meter_core_init(&c, &meter_demo_catalog, &bound));
    u.value.timestamp_ms = UINT32_MAX - 49;
    CHECK(meter_core_apply(&c, &u));
    meter_core_tick(&c, 750);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).state == METER_VALUE_STALE);
    u.value.value = NAN;
    CHECK(meter_core_apply(&c, &u));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).state == METER_VALUE_ERROR);
    /* 目录从未声明的身份代表未知数据，不是槽位号：在旧模型下 999
     * 恰好是平台定长数组的合法下标。 */
    u.signal = (meter_signal_id_t)999;
    CHECK(!meter_core_apply(&c, &u));
    u.signal = METER_ID_PRIVATE_FIRST;
    CHECK(!meter_core_apply(&c, &u));
    CHECK(meter_snapshot_read(&c.snapshot, METER_ID_PRIVATE_FIRST).state == METER_VALUE_UNKNOWN);
    CHECK(meter_core_parameter(&c, DEMO_PARAMETER_MAX_SPEED, 50));
    uint32_t revision = c.snapshot.revision;
    CHECK(meter_core_parameter(&c, DEMO_PARAMETER_MAX_SPEED, 50));
    CHECK(c.snapshot.revision == revision);
    CHECK(!meter_core_parameter(&c, DEMO_PARAMETER_MAX_SPEED, 51));
    CHECK(!meter_core_parameter(&c, DEMO_PARAMETER_MAX_SPEED, NAN));
    CHECK(!meter_core_parameter(&c, 999, 10));
    /* 故障状态用同样的方式寻址，产品无法触发自己不存在的条目。 */
    CHECK(meter_snapshot_fault_set(&c.snapshot, DEMO_FAULT_LOW_CHARGE, true));
    revision = c.snapshot.revision;
    CHECK(meter_snapshot_fault_set(&c.snapshot, DEMO_FAULT_LOW_CHARGE, true));
    CHECK(c.snapshot.revision == revision);
    CHECK(meter_snapshot_fault_active(&c.snapshot, DEMO_FAULT_LOW_CHARGE));
    CHECK(!meter_snapshot_fault_set(&c.snapshot, 999, true));
    /* 存储小于目录规模属于集成错误，必须在写入任何槽位之前被拦下。 */
    meter_core_storage_t tight = storage();
    tight.signal_capacity = meter_demo_catalog.signal_count - 1;
    CHECK(!meter_core_init(&c, &meter_demo_catalog, &tight));
    tight = storage();
    tight.fault_capacity = meter_demo_catalog.fault_count - 1;
    CHECK(!meter_core_init(&c, &meter_demo_catalog, &tight));
    tight = storage();
    tight.parameters = NULL;
    CHECK(!meter_core_init(&c, &meter_demo_catalog, &tight));
    /* 监控项只是展示信息，因此没有监控项的产品依然拥有合法的域。 */
    meter_catalog_t monitorless = meter_demo_catalog;
    monitorless.monitors = NULL;
    monitorless.monitor_count = 0;
    CHECK(meter_core_init(&c, &monitorless, &bound));
    meter_catalog_t ambiguous = meter_demo_catalog;
    ambiguous.signals = duplicate_signals;
    ambiguous.signal_count = SLOTS(duplicate_signals);
    ambiguous.monitors = NULL;
    ambiguous.monitor_count = 0;
    CHECK(!meter_core_init(&c, &ambiguous, &bound));
    ambiguous.signals = anonymous_signals;
    ambiguous.signal_count = SLOTS(anonymous_signals);
    CHECK(!meter_core_init(&c, &ambiguous, &bound));
    meter_catalog_t unresolved = meter_demo_catalog;
    unresolved.monitors = dangling_monitor;
    unresolved.monitor_count = SLOTS(dangling_monitor);
    CHECK(!meter_core_init(&c, &unresolved, &bound));
    meter_catalog_t shared = meter_demo_catalog;
    shared.faults = duplicate_faults;
    shared.fault_count = SLOTS(duplicate_faults);
    CHECK(!meter_core_init(&c, &shared, &bound));
    return 0;
}
static int runtime(void)
{
    meter_core_t c;
    meter_runtime_t r;
    meter_core_storage_t bound = storage();
    CHECK(meter_core_init(&c, &meter_demo_catalog, &bound));
    CHECK(meter_runtime_init(&r, &product, meter_core_apply, &c));
    meter_can_frame_t f = frame();
    CHECK(!meter_runtime_push(&r, &f));
    meter_runtime_connection(&r, true);
    for (unsigned i = 0; i < METER_RX_CAPACITY; ++i)
        CHECK(meter_runtime_push(&r, &f));
    CHECK(!meter_runtime_push(&r, &f));
    CHECK(r.diagnostics.overflow == 1);
    CHECK(meter_runtime_poll(&r, 3) == 3);
    CHECK(r.count == 29);
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).value == 25);
    uint32_t generation = r.generation;
    meter_runtime_connection(&r, false);
    CHECK(!r.count);
    CHECK(r.generation == generation + 1);
    meter_runtime_connection(&r, true);
    CHECK(!r.count);
    CHECK(meter_runtime_poll(&r, 32) == 0);
    f.id = 0x777;
    CHECK(meter_runtime_push(&r, &f));
    meter_runtime_poll(&r, 1);
    CHECK(r.diagnostics.unrouted == 1);
    f = frame();
    f.size = 7;
    CHECK(meter_runtime_push(&r, &f));
    meter_runtime_poll(&r, 1);
    CHECK(r.diagnostics.decode_failed == 1);
    f = frame();
    f.extended = true;
    f.id = 0x20000000;
    CHECK(!meter_runtime_push(&r, &f));
    CHECK(r.diagnostics.malformed == 1);
    CHECK(!meter_runtime_push(&r, NULL));
    CHECK(r.diagnostics.malformed == 2);
    CHECK(!r.count);
    meter_product_t bad = product;
    meter_protocol_binding_t dup[2] = {bindings[0], bindings[0]};
    meter_protocol_profile_t pp = {dup, 2};
    bad.protocols = &pp;
    CHECK(!meter_runtime_init(&r, &bad, meter_core_apply, &c));
    return 0;
}
static int protocol(void)
{
    meter_core_t c;
    meter_core_storage_t bound = storage();
    CHECK(meter_core_init(&c, &meter_demo_catalog, &bound));
    meter_can_frame_t f = frame();
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SPEED).value == 25);
    f.data[2] = 0x6c;
    f.data[3] = 0xee;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(meter_snapshot_read(&c.snapshot, METER_STEERING).value == -45);
    f.id = 0x101;
    f.data[0] = 101;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SOC).state == METER_VALUE_ERROR);
    f.data[0] = 100;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(meter_snapshot_read(&c.snapshot, METER_SOC).state == METER_VALUE_VALID);
    f.id = 0x102;
    f.data[0] = 0xff;
    f.data[1] = 0xff;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(meter_snapshot_read(&c.snapshot, METER_HEIGHT).state == METER_VALUE_ERROR);
    f = frame();
    f.size = 2;
    CHECK(!meter_demo_decode(&f, meter_core_apply, &c));
    f = frame();
    f.bus = METER_BUS_CAN1;
    CHECK(!meter_demo_decode(&f, meter_core_apply, &c));
    f = frame();
    f.extended = true;
    CHECK(!meter_demo_decode(&f, meter_core_apply, &c));
    return 0;
}
static int widgets(void)
{
    float a = 0;
    CHECK(meter_gauge_map(0, 0, 50, 135, 405, &a) && a == 135);
    CHECK(meter_gauge_map(25, 0, 50, 135, 405, &a) && a == 270);
    CHECK(meter_gauge_map(50, 0, 50, 135, 405, &a) && a == 405);
    CHECK(meter_gauge_map(500, 0, 50, 135, 405, &a) && a == 405);
    CHECK(meter_gauge_map(-5, 0, 50, 135, 405, &a) && a == 135);
    CHECK(!meter_gauge_map(NAN, 0, 50, 0, 180, &a));
    CHECK(!meter_gauge_map(1, 1, 1, 0, 180, &a));
    CHECK(meter_gauge_map(-45, -45, 45, 225, 315, &a) && a == 225);
    CHECK(meter_gauge_map(0, -45, 45, 225, 315, &a) && a == 270);
    CHECK(meter_gauge_map(45, -45, 45, 225, 315, &a) && a == 315);
    CHECK(meter_threshold_band(19, 20, 40) == 0);
    CHECK(meter_threshold_band(20, 20, 40) == 1);
    CHECK(meter_threshold_band(40, 20, 40) == 2);
    char text[64];
    meter_format_value(text, sizeof(text), 12.5f, METER_VALUE_UNKNOWN, "km/h", 1);
    CHECK(!strcmp(text, "-- km/h"));
    meter_format_value(text, sizeof(text), 12.5f, METER_VALUE_STALE, "km/h", 1);
    CHECK(strstr(text, "12.5") && strstr(text, "STALE"));
    meter_format_value(text, sizeof(text), NAN, METER_VALUE_VALID, "m", 1);
    CHECK(!strcmp(text, "ERR m"));
    return 0;
}
static int settings(void)
{
    meter_core_t a, b;
    meter_core_storage_t sa = storage(), sb = storage();
    CHECK(meter_core_init(&a, &meter_demo_catalog, &sa));
    CHECK(meter_core_init(&b, &meter_demo_catalog, &sb));
    meter_action_t u = {METER_ACTION_UNITS, 0, 1};
    CHECK(meter_core_action(&a, &u));
    u.kind = METER_ACTION_LANGUAGE;
    u.value = METER_LANGUAGE_ZH;
    CHECK(meter_core_action(&a, &u));
    CHECK(meter_core_parameter(&a, DEMO_PARAMETER_MAX_SPEED, 33));
    uint8_t bytes[128];
    /* 块长度随产品目录增长，而不是随平台容量上限增长。 */
    size_t size = meter_settings_size(&a);
    CHECK(size == METER_SETTINGS_OVERHEAD + meter_demo_catalog.parameter_count * METER_SETTINGS_ENTRY_SIZE);
    CHECK(!meter_settings_encode(&a, bytes, size - 1));
    CHECK(meter_settings_encode(&a, bytes, sizeof(bytes)));
    CHECK(meter_settings_decode(&b, bytes, size));
    float stored = 0;
    CHECK(b.snapshot.imperial && b.snapshot.language == METER_LANGUAGE_ZH && b.snapshot.brightness == 80);
    CHECK(meter_snapshot_parameter(&b.snapshot, DEMO_PARAMETER_MAX_SPEED, &stored) && stored == 33);
    bytes[8] ^= 1;
    CHECK(!meter_settings_decode(&b, bytes, size));
    /* 所有取值先整体校验再写入，被拒绝的文件不会让 core 停在半应用状态。
     * 结构体副本已检测不到这种问题：存储由调用方绑定后，两个副本共享同一组产品数组。 */
    CHECK(meter_snapshot_parameter(&b.snapshot, DEMO_PARAMETER_MAX_SPEED, &stored) && stored == 33);
    CHECK(b.snapshot.imperial && b.snapshot.language == METER_LANGUAGE_ZH && b.snapshot.brightness == 80);
    bytes[8] ^= 1;
    CHECK(!meter_settings_decode(&b, bytes, 4));
    CHECK(!meter_settings_decode(&b, bytes, size - 4));
    uint8_t foreign[128];
    memcpy(foreign, bytes, sizeof(foreign));
    foreign[6] = (uint8_t)(meter_demo_catalog.parameter_count - 1);
    CHECK(!meter_settings_decode(&b, foreign, METER_SETTINGS_OVERHEAD + foreign[6] * 4u));
    /* 格式完整但含一个越界参数的文件必须整体拒绝。第一项故意留在合法区间内，
     * 任何边校验边写入的解码器都会把它落进 core。 */
    uint8_t poisoned[128];
    memcpy(poisoned, bytes, sizeof(poisoned));
    store_float(poisoned + 16, 44.0f);
    store_float(poisoned + 24, 1000.0f);
    seal(poisoned, size);
    CHECK(!meter_settings_decode(&b, poisoned, size));
    CHECK(meter_snapshot_parameter(&b.snapshot, DEMO_PARAMETER_MAX_SPEED, &stored) && stored == 33);
    CHECK(meter_snapshot_parameter(&b.snapshot, meter_demo_catalog.parameters[1].id, &stored) &&
          stored == meter_demo_catalog.parameters[1].initial);
    u.value = 2;
    CHECK(!meter_core_action(&a, &u));
    return 0;
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    int result = 2;
    if (!strcmp(argv[1], "contracts"))
        result = contracts();
    if (!strcmp(argv[1], "core"))
        result = core();
    if (!strcmp(argv[1], "runtime"))
        result = runtime();
    if (!strcmp(argv[1], "protocol"))
        result = protocol();
    if (!strcmp(argv[1], "widgets"))
        result = widgets();
    if (!strcmp(argv[1], "settings"))
        result = settings();
    printf("%s: %s\n", argv[1], result ? "FAIL" : "PASS");
    return result;
}
