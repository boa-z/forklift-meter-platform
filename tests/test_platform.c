#include "core/meter_core.h"
#include "core/meter_settings.h"
#include "generated/demo_catalog.h"
#include "protocols/common/meter_frame_router.h"
#include "protocols/demo/demo_protocol.h"
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
    CHECK(meter_core_init(&c, &meter_demo_catalog));
    CHECK(c.snapshot.signals[METER_SPEED].state == METER_VALUE_UNKNOWN);
    meter_update_t u = {METER_SPEED, {12.5f, 100, METER_VALUE_VALID}};
    CHECK(meter_core_apply(&c, &u));
    meter_core_tick(&c, 849, 750);
    CHECK(c.snapshot.signals[METER_SPEED].state == METER_VALUE_VALID);
    meter_core_tick(&c, 850, 750);
    CHECK(c.snapshot.signals[METER_SPEED].state == METER_VALUE_STALE);
    CHECK(c.snapshot.signals[METER_SPEED].value == 12.5f);
    u.value.timestamp_ms = UINT32_MAX - 49;
    CHECK(meter_core_apply(&c, &u));
    meter_core_tick(&c, 50, 100);
    CHECK(c.snapshot.signals[METER_SPEED].state == METER_VALUE_STALE);
    u.value.value = NAN;
    CHECK(meter_core_apply(&c, &u));
    CHECK(c.snapshot.signals[METER_SPEED].state == METER_VALUE_ERROR);
    u.signal = (meter_signal_id_t)999;
    CHECK(!meter_core_apply(&c, &u));
    CHECK(meter_core_parameter(&c, 1, 50));
    CHECK(!meter_core_parameter(&c, 1, 51));
    CHECK(!meter_core_parameter(&c, 1, NAN));
    CHECK(!meter_core_parameter(&c, 999, 10));
    meter_catalog_t oversized = meter_demo_catalog;
    oversized.monitor_count = METER_MONITOR_CAPACITY + 1;
    CHECK(!meter_core_init(&c, &oversized));
    return 0;
}
static int runtime(void)
{
    meter_core_t c;
    meter_runtime_t r;
    CHECK(meter_core_init(&c, &meter_demo_catalog));
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
    CHECK(c.snapshot.signals[METER_SPEED].value == 25);
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
    CHECK(meter_core_init(&c, &meter_demo_catalog));
    meter_can_frame_t f = frame();
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(c.snapshot.signals[METER_SPEED].value == 25);
    f.data[2] = 0x6c;
    f.data[3] = 0xee;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(c.snapshot.signals[METER_STEERING].value == -45);
    f.id = 0x101;
    f.data[0] = 101;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(c.snapshot.signals[METER_SOC].state == METER_VALUE_ERROR);
    f.data[0] = 100;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(c.snapshot.signals[METER_SOC].state == METER_VALUE_VALID);
    f.id = 0x102;
    f.data[0] = 0xff;
    f.data[1] = 0xff;
    CHECK(meter_demo_decode(&f, meter_core_apply, &c));
    CHECK(c.snapshot.signals[METER_HEIGHT].state == METER_VALUE_ERROR);
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
    CHECK(meter_core_init(&a, &meter_demo_catalog));
    CHECK(meter_core_init(&b, &meter_demo_catalog));
    meter_action_t u = {METER_ACTION_UNITS, 0, 1};
    CHECK(meter_core_action(&a, &u));
    CHECK(meter_core_parameter(&a, 1, 33));
    uint8_t bytes[METER_SETTINGS_SIZE];
    CHECK(meter_settings_encode(&a, bytes));
    CHECK(meter_settings_decode(&b, bytes, sizeof(bytes)));
    CHECK(b.snapshot.imperial && b.snapshot.parameters[0] == 33);
    meter_core_t before = b;
    bytes[8] ^= 1;
    CHECK(!meter_settings_decode(&b, bytes, sizeof(bytes)));
    CHECK(!memcmp(&before, &b, sizeof(b)));
    CHECK(!meter_settings_decode(&b, bytes, 4));
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
