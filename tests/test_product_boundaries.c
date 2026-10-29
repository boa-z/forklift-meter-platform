#include "core/hmi_core.h"
#include "protocols/common/hmi_frame_router.h"
#include "runtime/hmi_periodic.h"
#include "runtime/hmi_runtime.h"
#include <assert.h>
#include <math.h>

static void test_overlapping_bus_and_format_identity(void)
{
    hmi_frame_route_t entries[] = {{HMI_BUS_CAN0, 0x123u, false, 1u},
                                     {HMI_BUS_CAN1, 0x123u, false, 2u},
                                     {HMI_BUS_CAN0, 0x123u, true, 3u}};
    const hmi_route_profile_t routes = {entries, 3u};
    assert(hmi_routes_valid(&routes));
    hmi_can_frame_t frame = {.bus = HMI_BUS_CAN0, .id = 0x123u, .size = 1u};
    assert(hmi_route_lookup(&routes, &frame)->owner == 1u);
    frame.bus = HMI_BUS_CAN1;
    assert(hmi_route_lookup(&routes, &frame)->owner == 2u);
    frame.extended = true;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    frame.bus = HMI_BUS_CAN0;
    assert(hmi_route_lookup(&routes, &frame)->owner == 3u);
    frame.remote = true;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    entries[1].bus = HMI_BUS_CAN0;
    assert(!hmi_routes_valid(&routes));
}

static void test_fallback_is_scoped_and_exact_routes_win(void)
{
    hmi_frame_route_t entries[] = {{HMI_BUS_CAN0, HMI_CAN_ID_ANY, false, 1u},
                                    {HMI_BUS_CAN0, 0x123u, false, 2u},
                                    {HMI_BUS_CAN1, HMI_CAN_ID_ANY, true, 3u}};
    const hmi_route_profile_t routes = {entries, 3u};
    assert(hmi_routes_valid(&routes));
    hmi_can_frame_t frame = {.bus = HMI_BUS_CAN0, .id = 0x123u, .size = 8u};
    assert(hmi_route_lookup(&routes, &frame)->owner == 2u);
    frame.id = 0x456u;
    assert(hmi_route_lookup(&routes, &frame)->owner == 1u);
    frame.extended = true;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    frame.bus = HMI_BUS_CAN1;
    assert(hmi_route_lookup(&routes, &frame)->owner == 3u);
    frame.extended = false;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    frame.bus = HMI_BUS_CAN0;
    frame.id = 0x800u;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    frame.id = 0x456u;
    frame.size = 9u;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    frame.size = 8u;
    frame.remote = true;
    assert(hmi_route_lookup(&routes, &frame) == NULL);
    entries[1].id = HMI_CAN_ID_ANY;
    assert(!hmi_routes_valid(&routes));
}

static void test_timeout_keeps_source_value_and_recovers(void)
{
    const hmi_signal_def_t signals[] = {{.id = 1u, .stale_ms = 20u}, {.id = 2u, .stale_ms = 50u}};
    const hmi_catalog_t catalog = {.signals = signals, .signal_count = 2u};
    hmi_value_t values[2];
    const hmi_core_storage_t storage = {.signals = values, .signal_capacity = 2u};
    hmi_core_t core;
    assert(hmi_core_init(&core, &catalog, &storage));
    assert(values[0].state == HMI_VALUE_UNKNOWN && values[0].source == HMI_SOURCE_NONE);
    const uint32_t start = UINT32_MAX - 10u;
    hmi_update_t first = {1u, {42.0f, start, HMI_VALUE_VALID, 11u}};
    hmi_update_t second = {2u, {17.0f, start, HMI_VALUE_VALID, 12u}};
    assert(hmi_core_apply(&core, &first));
    assert(hmi_core_apply(&core, &second));
    hmi_core_tick(&core, start + 19u);
    assert(values[0].state == HMI_VALUE_VALID);
    hmi_core_tick(&core, start + 20u);
    assert(values[0].state == HMI_VALUE_STALE && values[1].state == HMI_VALUE_VALID);
    assert(values[0].value == 42.0f && values[0].timestamp_ms == start && values[0].source == 11u);
    hmi_core_tick(&core, start + 50u);
    assert(values[1].state == HMI_VALUE_STALE && values[1].value == 17.0f && values[1].source == 12u);
    first.value.value = 43.0f;
    first.value.timestamp_ms = start + 51u;
    assert(hmi_core_apply(&core, &first));
    assert(values[0].state == HMI_VALUE_VALID && values[0].value == 43.0f && values[0].source == 11u);
    hmi_core_connection(&core, false, 2u);
    assert(values[0].state == HMI_VALUE_STALE && values[0].value == 43.0f);
    hmi_core_connection(&core, true, 3u);
    assert(values[0].state == HMI_VALUE_STALE); /* Connection alone is not new data. */
    first.value.value = NAN;
    assert(hmi_core_apply(&core, &first));
    assert(values[0].state == HMI_VALUE_ERROR); /* Never a valid zero; no separate last-good history. */
}

/* 帧在运行时一轮里按读取时刻打戳，比这一轮开头取的 now 更晚几毫秒：这样的值不是过期，更不能因无符号回绕被判成过期。 */
static void test_timestamp_slightly_ahead_of_tick_is_not_stale(void)
{
    const hmi_signal_def_t signals[] = {{.id = 1u, .stale_ms = 500u}};
    const hmi_catalog_t catalog = {.signals = signals, .signal_count = 1u};
    hmi_value_t values[1];
    const hmi_core_storage_t storage = {.signals = values, .signal_capacity = 1u};
    hmi_core_t core;
    assert(hmi_core_init(&core, &catalog, &storage));
    for (uint32_t now = UINT32_MAX - 3u; now != 6u; ++now) /* 含无符号回绕 */
    {
        hmi_update_t update = {1u, {1.0f, now + 2u, HMI_VALUE_VALID, 11u}};
        assert(hmi_core_apply(&core, &update));
        hmi_core_tick(&core, now);
        assert(values[0].state == HMI_VALUE_VALID);
    }
    hmi_update_t update = {1u, {1.0f, 1000u, HMI_VALUE_VALID, 11u}};
    assert(hmi_core_apply(&core, &update));
    hmi_core_tick(&core, 1499u);
    assert(values[0].state == HMI_VALUE_VALID);
    hmi_core_tick(&core, 1500u); /* 期限不变：恰好 stale_ms 之后过期 */
    assert(values[0].state == HMI_VALUE_STALE);
}

static void test_independent_twenty_and_fifty_ms_periods(void)
{
    const hmi_periodic_frame_t fast = {.frame = {.bus = HMI_BUS_CAN0, .id = 0x123u}, .period_ms = 20u};
    const hmi_periodic_frame_t slow = {.frame = {.bus = HMI_BUS_CAN1, .id = 0x123u}, .period_ms = 50u};
    hmi_deadline_t fast_state = {0}, slow_state = {0};
    hmi_can_frame_t frame;
    /* 第一帧在启用时立即发出，之后两个条目各按自己的周期。 */
    assert(hmi_periodic_due(&fast_state, &fast, NULL, 0u, true, false, &frame) && frame.bus == HMI_BUS_CAN0);
    assert(hmi_periodic_due(&slow_state, &slow, NULL, 0u, true, false, &frame) && frame.bus == HMI_BUS_CAN1);
    assert(!hmi_periodic_due(&fast_state, &fast, NULL, 19u, true, false, &frame));
    assert(hmi_periodic_due(&fast_state, &fast, NULL, 20u, true, false, &frame) && frame.timestamp_ms == 20u);
    assert(!hmi_periodic_due(&slow_state, &slow, NULL, 49u, true, false, &frame));
    assert(hmi_periodic_due(&slow_state, &slow, NULL, 50u, true, false, &frame));
    /* 迟到只补一帧并记录跳过的周期，不补发突发。 */
    assert(hmi_periodic_due(&fast_state, &fast, NULL, 100u, true, false, &frame));
    assert(fast_state.missed == 3u && fast_state.next_ms == 120u);
    assert(!hmi_periodic_due(&fast_state, &fast, NULL, 100u, true, false, &frame));
    assert(hmi_periodic_due(&slow_state, &slow, NULL, 100u, true, false, &frame) && slow_state.missed == 0u);
    /* 不允许发送或上一帧仍在途：期限照常推进，不生成帧。 */
    assert(!hmi_periodic_due(&slow_state, &slow, NULL, 150u, false, false, &frame) && slow_state.next_ms == 200u);
    assert(!hmi_periodic_due(&slow_state, &slow, NULL, 200u, true, true, &frame) && slow_state.next_ms == 250u);
}

static bool encode_level(const hmi_snapshot_t *snapshot, uint32_t now, hmi_can_frame_t *frame)
{
    (void)now;
    const hmi_value_t level = hmi_snapshot_read(snapshot, 1u);
    /* 值不可用时本周期不发；可用时把值写进模板的第二个字节。 */
    if (level.state != HMI_VALUE_VALID)
        return false;
    frame->data[1] = (uint8_t)level.value;
    return true;
}

static void test_encoder_reads_the_snapshot(void)
{
    static const hmi_signal_def_t signals[] = {{"level", "%", 0u, 1u}};
    static const hmi_catalog_t catalog = {.signals = signals, .signal_count = 1u};
    hmi_value_t values[1];
    hmi_core_storage_t storage = {.signals = values, .signal_capacity = 1u};
    hmi_core_t core;
    assert(hmi_core_init(&core, &catalog, &storage));
    const hmi_periodic_frame_t definition = {
        .frame = {.bus = HMI_BUS_CAN0, .id = 0x3c0u, .size = 2u, .data = {0xA5u}}, .period_ms = 10u,
        .encode = encode_level};
    assert(hmi_periodic_valid(&definition));
    hmi_deadline_t deadline = {0};
    hmi_can_frame_t frame;
    assert(!hmi_periodic_due(&deadline, &definition, &core.snapshot, 0u, true, false, &frame));
    const hmi_update_t level = {1u, {37.0f, 5u, HMI_VALUE_VALID, 1u}};
    assert(hmi_core_apply(&core, &level));
    assert(hmi_periodic_due(&deadline, &definition, &core.snapshot, 10u, true, false, &frame));
    assert(frame.id == 0x3c0u && frame.size == 2u && frame.data[0] == 0xA5u && frame.data[1] == 37u);
    const hmi_periodic_frame_t broken = {.frame = {.bus = HMI_BUS_COUNT}, .period_ms = 10u};
    assert(!hmi_periodic_valid(&broken));
}

static bool sink(void *context, const hmi_update_t *update)
{
    (void)context;
    (void)update;
    return true;
}

/* 只有本地业务和 UI 的 Product：没有协议、没有路由，目录也可以没有信号。 */
static void test_product_without_protocols_or_signals(void)
{
    static const hmi_catalog_t empty = {0};
    hmi_core_storage_t none = {0};
    hmi_core_t core;
    assert(hmi_core_init(&core, &empty, &none));
    static const hmi_product_t local = {.id = "local", .catalog = &empty};
    hmi_runtime_t runtime;
    assert(hmi_runtime_init(&runtime, &local, sink, NULL));
    hmi_runtime_connection(&runtime, true);
    hmi_can_frame_t frame = {.bus = HMI_BUS_CAN0, .id = 0x100u, .size = 1u};
    assert(!hmi_runtime_receive(&runtime, &frame));
    assert(runtime.diagnostics.unrouted == 1u);
    const hmi_command_t command = {.id = 1u};
    assert(!hmi_runtime_command(&runtime, &command));
    hmi_runtime_step(&runtime, &core, 10u, true);
}

/* app_tick 用 hmi_snapshot_write 发布本地算出的值：只有内容变化才推进 revision。 */
static void test_local_value_publication(void)
{
    static const hmi_signal_def_t signals[] = {{.key = "local.count", .unit = "", .id = 7u}};
    static const hmi_catalog_t catalog = {.signals = signals, .signal_count = 1u};
    hmi_value_t slots[1];
    hmi_core_storage_t storage = {.signals = slots, .signal_capacity = 1u};
    hmi_core_t core;
    assert(hmi_core_init(&core, &catalog, &storage));
    const uint32_t revision = core.snapshot.revision;
    assert(hmi_snapshot_write(&core.snapshot, 7u, 3.0f, 100u));
    assert(core.snapshot.revision == revision + 1u);
    assert(hmi_snapshot_write(&core.snapshot, 7u, 3.0f, 200u));
    assert(core.snapshot.revision == revision + 1u);
    const hmi_value_t value = hmi_snapshot_read(&core.snapshot, 7u);
    assert(value.state == HMI_VALUE_VALID && value.value == 3.0f && value.timestamp_ms == 200u);
    assert(!hmi_snapshot_write(&core.snapshot, 8u, 1.0f, 300u));
}

int main(void)
{
    test_product_without_protocols_or_signals();
    test_local_value_publication();
    test_encoder_reads_the_snapshot();
    test_overlapping_bus_and_format_identity();
    test_fallback_is_scoped_and_exact_routes_win();
    test_timeout_keeps_source_value_and_recovers();
    test_timestamp_slightly_ahead_of_tick_is_not_stale();
    test_independent_twenty_and_fifty_ms_periods();
    return 0;
}
