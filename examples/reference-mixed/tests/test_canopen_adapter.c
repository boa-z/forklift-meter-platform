#include "canopen/mixed_canopen.h"
#include "catalog/catalog.h"
#include "core/meter_core.h"
#include "runtime/meter_runtime.h"
#include "sdo_peer.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                       \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static sdo_peer_t peer;
static meter_core_t core;
static meter_runtime_t runtime;
static meter_value_t values[4];
static float params[2];
static unsigned sync_events, timeout_events, failure_events, command_events;
static uint32_t now;
static bool event(void *ctx, const meter_protocol_event_t *e)
{
    (void)ctx;
    if (e->id == MIXED_EVENT_SYNC_DONE)
        ++sync_events;
    if (e->id == MIXED_EVENT_PDO_TIMEOUT)
        ++timeout_events;
    if (e->id == MIXED_EVENT_SDO_FAILED)
        ++failure_events;
    if (e->id == MIXED_EVENT_SDO_COMPLETE)
        ++command_events;
    return true;
}
static void pump(void)
{
    (void)meter_runtime_process(&runtime, now++);
    sdo_peer_step(&peer);
    for (unsigned i = 0; i < peer.out_count; ++i)
        (void)meter_runtime_push(&runtime, &peer.outgoing[i]);
    peer.out_count = 0;
    (void)meter_runtime_poll(&runtime, 32);
}
static bool pdo(uint32_t timestamp)
{
    meter_can_frame_t f = {.bus = METER_BUS_CAN1,
                           .id = 0x20c,
                           .size = 8,
                           .timestamp_ms = timestamp,
                           .data = {65, 0, 0xe2, 0xff, 0, 0, 0, 0}};
    return meter_runtime_push(&runtime, &f) && meter_runtime_poll(&runtime, 32) == 1;
}
int main(void)
{
    const meter_product_t *product = meter_product_get();
    meter_core_storage_t storage = {
        .signals = values, .signal_capacity = 4, .parameters = params, .parameter_capacity = 2};
    CHECK(meter_core_init(&core, &mixed_catalog, &storage));
    CHECK(meter_runtime_init(&runtime, product, meter_core_apply, &core));
    CHECK(sdo_peer_init(&peer));
    meter_can_tx_port_t tx = {sdo_peer_send, &peer};
    meter_runtime_bind_services(&runtime, event, NULL, &tx);
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(meter_canopen_profile_pdo_sdo_only(&mixed_canopen_profile));
    for (now = 0; now < 10;)
        pump();
    CHECK(peer.sent_count == 0 && mixed_canopen_state.startup.phase == MIXED_SYNC_WAIT_DATA);
    CHECK(pdo(now));
    CHECK(meter_snapshot_read(&core.snapshot, MIXED_PDO_SPEED).value == 6.5f);
    CHECK(meter_snapshot_read(&core.snapshot, MIXED_PDO_TORQUE).value == -15.0f);
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(mixed_canopen_state.startup.phase == MIXED_SYNC_READY && sync_events == 1);
    CHECK(mixed_canopen_state.startup.parameters[0] == 250 &&
          mixed_canopen_state.startup.parameters[1] == 30);
    CHECK(peer.sent_count == 2 && peer.sent[0].data[3] == 1 && peer.sent[1].data[3] == 2);
    /* 应用只提交业务命令，Product router 与 Adapter 负责请求排队。 */
    meter_command_t cmd = {MIXED_CMD_SET_MAX_SPEED, 31.2f, 0};
    unsigned first = peer.sent_count;
    CHECK(meter_runtime_command(&runtime, &cmd));
    CHECK(peer.sent_count == first);
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.parameter_a == 312 && peer.sent[first].data[0] == 0x2b && command_events == 1);
    cmd.id = MIXED_CMD_SET_ACCEL;
    cmd.value = 4.2f;
    CHECK(meter_runtime_command(&runtime, &cmd));
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.parameter_b == 42 && command_events == 2);
    cmd.id = 999;
    CHECK(!meter_runtime_command(&runtime, &cmd));
    /* PDO 超时取消 SDO 与启动流程，不依赖 Heartbeat；恢复后重新同步。 */
    now = 600;
    pump();
    CHECK(timeout_events == 1 && mixed_canopen_state.timeout_reported);
    CHECK(mixed_canopen_state.startup.phase == MIXED_SYNC_WAIT_DATA);
    pump();
    CHECK(timeout_events == 1);
    CHECK(pdo(now));
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(!mixed_canopen_state.timeout_reported && sync_events == 2);
    CHECK(mixed_canopen_state.startup.parameters[0] == 312);
    /* 上游 abort 不能跳过 A 直接读取 B，更不能错误进入 READY。 */
    meter_runtime_connection(&runtime, false);
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(sdo_peer_init(&peer));
    peer.entries[0].subEntriesCount = 1;
    CHECK(pdo(now));
    for (unsigned i = 0; i < 20; ++i)
        pump();
    CHECK(mixed_canopen_state.startup.phase == MIXED_SYNC_FAILED && failure_events >= 1);
    CHECK(peer.sent_count == 1 && sync_events == 2);
    /* 持续收到 PDO 时 SDO 自己耗尽重试；无成功响应不会进入下一步。 */
    meter_runtime_connection(&runtime, false);
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(sdo_peer_init(&peer));
    peer.drop = true;
    CHECK(pdo(now));
    for (unsigned i = 0; i < 1100; ++i)
    {
        if (i % 100 == 0)
            CHECK(pdo(now));
        pump();
    }
    CHECK(mixed_canopen_state.startup.phase == MIXED_SYNC_FAILED);
    CHECK(mixed_canopen_state.startup.error == CO_SDO_AB_TIMEOUT && sync_events == 2);
    /* 真正断连会复位整个适配器；重连后仍须等待有效 PDO。 */
    meter_runtime_connection(&runtime, false);
    CHECK(!meter_runtime_process(&runtime, now));
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(sdo_peer_init(&peer));
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.sent_count == 0 && mixed_canopen_state.startup.phase == MIXED_SYNC_WAIT_DATA);
    CHECK(pdo(now));
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(mixed_canopen_state.startup.phase == MIXED_SYNC_READY);
    /* 所有发出帧只能属于当前 SDO 通道，测试无需任何 NMT 或 Heartbeat 帧。 */
    for (unsigned i = 0; i < peer.sent_count; ++i)
        CHECK(peer.sent[i].id == 0x60c && peer.sent[i].bus == METER_BUS_CAN1);
    puts("Mixed PDO freshness/recovery; upstream SDO startup; product command routing; failure/reset PASS");
    return 0;
}
