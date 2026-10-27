/* 固件运行时端口的确定性测试：真实端口代码 + 无调度器的 RT-Thread 替身，逐轮调用 app_step/tx_service。 */
#include "platform/rtthread/meter_execution_port.c"
#include "tests/stubs/execution/fixture.h"

static unsigned resets, ticks, frames_seen, commands_seen, encodes;
static uint8_t counter;
static bool on_frame(void *ctx, const meter_can_frame_t *f, const meter_protocol_services_t *s)
{
    (void)ctx;
    ++frames_seen;
    meter_update_t u = {1u, {(float)f->data[0], f->timestamp_ms, METER_VALUE_VALID, 1u}};
    return s->update(s->update_context, &u);
}
static bool command(void *ctx, const meter_command_t *c, const meter_protocol_services_t *s)
{
    (void)ctx;
    ++commands_seen;
    meter_can_frame_t f = {.bus = METER_BUS_CAN0, .id = 0x600u + c->id, .size = 1u};
    return s->tx->send(s->tx->context, &f);
}
static void adapter_reset(void *ctx)
{
    (void)ctx;
}
static bool route(void *ctx, const meter_command_t *c, meter_frame_route_owner_t *owner)
{
    (void)ctx;
    (void)c;
    *owner = 1u;
    return true;
}
static void app_reset(uint32_t generation)
{
    (void)generation;
    ++resets;
}
static void app_run(uint32_t now, const meter_command_port_t *commands)
{
    /* 命令同步交给适配器：同一轮里连发两条都应被接受。 */
    if (now == 100u)
    {
        meter_command_t a = {.id = 1u}, b = {.id = 2u};
        assert(commands->send(commands->context, &a) && commands->send(commands->context, &b));
    }
}
static void app_tick(meter_snapshot_t *snapshot, uint32_t now)
{
    (void)snapshot;
    (void)now;
    ++ticks;
}
static bool encode(const meter_snapshot_t *snapshot, uint32_t now, meter_can_frame_t *frame)
{
    (void)now;
    ++encodes;
    frame->data[0] = (uint8_t)meter_snapshot_read(snapshot, 1u).value;
    frame->data[1] = counter++;
    return true;
}
static meter_execution_liveness_t live(void)
{
    meter_execution_liveness_t l;
    assert(meter_execution_liveness(&l));
    return l;
}
int main(void)
{
    static const meter_signal_def_t signal[] = {{.id = 1u, .key = "a", .unit = "", .stale_ms = 0u}};
    static const meter_catalog_t catalog = {.signals = signal, .signal_count = 1u};
    static const meter_protocol_adapter_t adapter = {.on_frame = on_frame, .command = command, .reset = adapter_reset};
    static const meter_protocol_binding_t binding[] = {{.owner = 1u, .adapter = &adapter}};
    static const meter_protocol_profile_t profile = {binding, 1u};
    static const meter_frame_route_t entry[] = {{METER_BUS_CAN0, 0x100u, false, 1u}};
    static const meter_route_profile_t routes = {entry, 1u};
    static const meter_periodic_frame_t periodic[] = {
        {.frame = {.bus = METER_BUS_CAN0, .id = 0x303u, .size = 8u}, .period_ms = 50u, .encode = encode}};
    static const meter_product_t product = {.catalog = &catalog, .protocols = &profile, .routes = &routes,
        .command_route = route, .periodic = periodic, .periodic_count = 1u, .app_reset = app_reset,
        .app_run = app_run, .app_tick = app_tick};
    static meter_core_t core;
    static meter_value_t values[3][1];
    static meter_diagnostics_t diagnostics;
    meter_core_storage_t domain = {.signals = values[0], .signal_capacity = 1u};
    meter_core_storage_t presented = {.signals = values[1], .signal_capacity = 1u};
    meter_core_storage_t ui = {.signals = values[2], .signal_capacity = 1u};
    assert(meter_core_init(&core, &catalog, &domain));
    meter_execution_config_t c = {.product = &product, .core = &core, .published_storage = presented,
                                  .public_diagnostics = &diagnostics};

    /* 启动前不接收动作，看门狗不监督。 */
    meter_action_t brightness = {.kind = METER_ACTION_BRIGHTNESS, .value = 30.0f};
    meter_execution_liveness_t l;
    assert(meter_execution_start(&c) && !meter_execution_action(NULL, &brightness) && !meter_execution_liveness(&l));
    /* 启动：只打开 Product 用到的总线，速率取启动时锁存值；进入第一个会话。 */
    assert(app_start() && opened[METER_BUS_CAN0] == 1u && opened[METER_BUS_CAN1] == 0u && opened_bitrate == 250000u);
    assert(resets == 1u && generation != 0u && core.snapshot.connected);
    observed_revision = core.snapshot.revision;
    publish();
    started = true;
    meter_snapshot_t view = {0};
    meter_update_view_t update;
    bool normal = false;
    assert(meter_execution_present(&view, &ui, &update, &normal) && normal);
    assert(!strcmp(update.current_version, METER_FIRMWARE_DISPLAY_VERSION));

    /* 一轮里：收帧直接写进 Core；周期帧立刻按快照编码入队；同一条目在途时不再排下一帧。 */
    rx_queue[rx_count++] = (meter_can_frame_t){.bus = METER_BUS_CAN0, .id = 0x100u, .size = 1u, .data = {42u}};
    fake_now = 10u;
    assert(app_step(fake_now) == 1u && frames_seen == 1u && meter_snapshot_read(&core.snapshot, 1u).value == 42.0f);
    assert(encodes == 1u && periodic_in_flight[0] && ticks == 1u && changes == 1u);
    assert(meter_execution_present(&view, &ui, &update, &normal) && meter_snapshot_read(&view, 1u).value == 42.0f);
    fake_now = 40u;
    (void)app_step(fake_now);
    assert(encodes == 1u);
    assert(tx_service(&tx[METER_BUS_CAN0]) && sends == 1u && sent[0].id == 0x303u && sent[0].data[0] == 42u);
    assert(!periodic_in_flight[0] && !tx_service(&tx[METER_BUS_CAN0]));
    fake_now = 60u;
    (void)app_step(fake_now);
    assert(encodes == 2u && periodic_in_flight[0]);
    fake_now = 110u;
    (void)app_step(fake_now);
    assert(encodes == 2u && periodic_deadline[0].missed == 0u);
    /* 超过一个周期还没交给驱动的周期帧作废，不积压。 */
    fake_now = 111u;
    assert(tx_service(&tx[METER_BUS_CAN0]) && sends == 1u && tx[METER_BUS_CAN0].dropped == 1u);

    /* 命令端口同步：app_run 一轮连发两条，适配器都接受且帧都入队。 */
    fake_now = 100u;
    periodic_deadline[0].next_ms = 1000u;
    (void)app_step(fake_now);
    assert(commands_seen == 2u);
    assert(tx_service(&tx[METER_BUS_CAN0]) && tx_service(&tx[METER_BUS_CAN0]) && sends == 3u);
    assert(sent[1].id == 0x601u && sent[2].id == 0x602u);

    /* 动作队列不是单信用：UI 连发两个动作都能入队，下一轮按序应用。 */
    meter_action_t units = {.kind = METER_ACTION_UNITS, .value = 1.0f};
    assert(meter_execution_action(NULL, &brightness) && meter_execution_action(NULL, &units));
    fake_now = 200u;
    (void)app_step(fake_now);
    assert(core.snapshot.brightness == 30u && core.snapshot.imperial && actions_rejected == 0u);
    meter_action_t invalid = {.kind = METER_ACTION_BRIGHTNESS, .value = 5.0f};
    assert(meter_execution_action(NULL, &invalid));
    (void)app_step(fake_now);
    assert(actions_rejected == 1u && core.snapshot.brightness == 30u);

    /* 设置尚未加载完成时不接收动作。 */
    nvm_ready = false;
    assert(!meter_execution_action(NULL, &brightness));
    nvm_ready = true;

    /* 会话切换：旧代次里还没发出的协议帧作废，Product 工作流复位。 */
    meter_command_t late = {.id = 3u};
    assert(meter_runtime_command(&runtime, &late));
    restart_session();
    assert(resets == 2u && tx_service(&tx[METER_BUS_CAN0]) && sends == 3u && tx[METER_BUS_CAN0].dropped == 2u);
    /* 升级帧不属于会话，代次切换后照常发送。 */
    meter_can_frame_t uds = {.bus = METER_BUS_CAN0, .id = 0x7e8u, .size = 8u};
    assert(meter_execution_can_submit(&uds, true));
    restart_session();
    assert(tx_service(&tx[METER_BUS_CAN0]) && sends == 4u && sent[3].id == 0x7e8u);
    /* 未打开的总线不接收帧。 */
    uds.bus = METER_BUS_CAN1;
    assert(!meter_execution_can_submit(&uds, true));

    /* 看门狗心跳：运行时一轮推进 app_runs，UI 每次发布统计推进 ui_ticks。 */
    uint32_t runs = live().app_runs;
    (void)app_step(fake_now);
    meter_diagnostics_t ui_diag = {0};
    meter_execution_ui_diagnostics(&ui_diag);
    assert(live().app_runs == runs + 1u && live().ui_ticks == 1u);
    return 0;
}
