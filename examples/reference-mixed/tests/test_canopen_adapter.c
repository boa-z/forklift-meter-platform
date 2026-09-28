#include "canopen/mixed_canopen.h"
#include "catalog/catalog.h"
#include "services/startup_parameter_sync.h"
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
static uint32_t now, app_generation;
static meter_protocol_event_t app_events[64];
static unsigned event_count;
static uint64_t serial;
static meter_request_id_t request_id;
static meter_command_stage_t request_stage;
static bool reserved;
static void progress(void *ctx, meter_request_id_t id, meter_command_stage_t stage)
{ (void)ctx; if (meter_request_id_equal(id, request_id)) request_stage = stage; }
static meter_request_admission_t submit(void *ctx, const meter_command_t *cmd, uint32_t timeout, meter_request_id_t *id)
{
    (void)ctx; (void)timeout;
    if (reserved) return METER_REQUEST_BUSY;
    request_id = (meter_request_id_t){runtime.generation, ++serial}; *id = request_id;
    request_stage = METER_COMMAND_QUEUED; reserved = true;
    (void)meter_runtime_command_request(&runtime, cmd, request_id, progress, NULL);
    return METER_REQUEST_QUEUED;
}
static bool take(void *ctx, meter_request_id_t id, meter_command_stage_t *stage, bool ack)
{
    (void)ctx;
    if (!reserved || !meter_request_id_equal(id, request_id)) return false;
    *stage = request_stage;
    if (ack) reserved = false;
    return true;
}
static const meter_command_port_t commands = {.submit = submit, .result = take};
static meter_diagnostics_t diagnostics;
static bool event(void *ctx, const meter_protocol_event_t *e)
{
    (void)ctx;
    if (event_count >= 64u) return false;
    app_events[event_count++] = *e;
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
    const meter_product_t *product = meter_product_get();
    if (app_generation != runtime.generation)
    { product->app_reset(runtime.generation); app_generation = runtime.generation; if (reserved) request_stage = METER_COMMAND_CANCELLED; }
    (void)meter_runtime_process(&runtime, now++);
    sdo_peer_step(&peer);
    for (unsigned i = 0; i < peer.out_count; ++i)
        (void)meter_runtime_push(&runtime, &peer.outgoing[i]);
    peer.out_count = 0;
    (void)meter_runtime_poll(&runtime, 32);
    for (unsigned i = 0; i < event_count; ++i) product->on_event(&app_events[i]);
    event_count = 0;
    mixed_sync_phase_t before = mixed_startup.phase;
    product->app_run(now, &commands);
    if (before != MIXED_SYNC_READY && mixed_startup.phase == MIXED_SYNC_READY) ++sync_events;
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
    meter_diagnostics_init(&diagnostics);
    meter_runtime_bind_diagnostics(&runtime, &diagnostics);
    meter_core_bind_diagnostics(&core, &diagnostics);
    CHECK(sdo_peer_init(&peer));
    meter_can_tx_port_t tx = {sdo_peer_send, &peer};
    meter_runtime_bind_services(&runtime, event, NULL, &tx);
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(meter_canopen_profile_pdo_sdo_only(&mixed_canopen_profile));
    for (now = 0; now < 10;)
        pump();
    CHECK(peer.sent_count == 0 && mixed_startup.phase == MIXED_SYNC_WAIT_DATA);
    CHECK(pdo(now));
    CHECK(meter_snapshot_read(&core.snapshot, MIXED_PDO_SPEED).value == 6.5f);
    CHECK(meter_snapshot_read(&core.snapshot, MIXED_PDO_TORQUE).value == -15.0f);
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(mixed_startup.phase == MIXED_SYNC_READY && sync_events == 1);
    CHECK(mixed_startup.parameters[0] == 250 &&
          mixed_startup.parameters[1] == 30);
    CHECK(peer.sent_count == 2 && peer.sent[0].data[3] == 1 && peer.sent[1].data[3] == 2);
    /* 应用只提交业务命令，Product router 与 Adapter 负责请求排队。 */
    meter_command_t cmd = {MIXED_CMD_SET_MAX_SPEED, 31.2f, 0};
    unsigned first = peer.sent_count;
    CHECK(meter_runtime_command(&runtime, &cmd));
    CHECK(peer.sent_count == first);
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.parameter_a == 312 && peer.sent[first].data[0] == 0x2b && command_events == 3);
    cmd.id = MIXED_CMD_SET_ACCEL;
    cmd.value = 4.2f;
    CHECK(meter_runtime_command(&runtime, &cmd));
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.parameter_b == 42 && command_events == 4);
    cmd.id = 999;
    CHECK(!meter_runtime_command(&runtime, &cmd));
    /* PDO 超时取消 SDO 与启动流程，不依赖 Heartbeat；恢复后重新同步。 */
    now = 600;
    pump();
    CHECK(timeout_events == 1 && mixed_canopen_state.timeout_reported);
    CHECK(diagnostics.data.pdo.stale == 1 && diagnostics.data.pdo.stale_state);
    CHECK(mixed_startup.phase == MIXED_SYNC_WAIT_DATA);
    pump();
    CHECK(timeout_events == 1);
    CHECK(pdo(now));
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(!mixed_canopen_state.timeout_reported && sync_events == 2);
    CHECK(diagnostics.data.pdo.recover == 1 && !diagnostics.data.pdo.stale_state);
    CHECK(diagnostics.data.sdo.completed == 6 && diagnostics.data.sdo.reset == 1);
    CHECK(mixed_startup.parameters[0] == 312);
    /* 上游 abort 不能跳过 A 直接读取 B，更不能错误进入 READY。 */
    meter_runtime_connection(&runtime, false);
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(sdo_peer_init(&peer));
    peer.entries[0].subEntriesCount = 1;
    CHECK(pdo(now));
    for (unsigned i = 0; i < 20; ++i)
        pump();
    CHECK(mixed_startup.phase == MIXED_SYNC_FAILED && failure_events >= 1);
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
    CHECK(mixed_startup.phase == MIXED_SYNC_FAILED);
    CHECK(mixed_startup.error == CO_SDO_AB_TIMEOUT && sync_events == 2);
    /* 真正断连会复位整个适配器；重连后仍须等待有效 PDO。 */
    meter_runtime_connection(&runtime, false);
    CHECK(!meter_runtime_process(&runtime, now));
    meter_runtime_connection(&runtime, true);
    mixed_canopen_state.tpdo_armed = false;
    CHECK(sdo_peer_init(&peer));
    for (unsigned i = 0; i < 10; ++i)
        pump();
    CHECK(peer.sent_count == 0 && mixed_startup.phase == MIXED_SYNC_WAIT_DATA);
    CHECK(pdo(now));
    for (unsigned i = 0; i < 30; ++i)
        pump();
    CHECK(mixed_startup.phase == MIXED_SYNC_READY);
    /* 所有发出帧只能属于当前 SDO 通道，测试无需任何 NMT 或 Heartbeat 帧。 */
    for (unsigned i = 0; i < peer.sent_count; ++i)
        CHECK(peer.sent[i].id == 0x60c && peer.sent[i].bus == METER_BUS_CAN1);
    puts("Mixed PDO freshness/recovery; upstream SDO startup; product command routing; failure/reset PASS");
    return 0;
}
