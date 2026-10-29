/* 固件运行时端口的确定性测试：真实端口代码 + 无调度器的 RT-Thread 替身，逐轮调用 app_step/tx_service。 */
#include "platform/rtthread/hmi_execution_port.c"
#include "tests/stubs/execution/fixture.h"

static unsigned resets, ticks, frames_seen, commands_seen, encodes;
static uint8_t counter;
static bool on_frame(void *ctx, const hmi_can_frame_t *f, const hmi_protocol_services_t *s)
{
    (void)ctx;
    ++frames_seen;
    hmi_update_t u = {1u, {(float)f->data[0], f->timestamp_ms, HMI_VALUE_VALID, 1u}};
    return s->update(s->update_context, &u);
}
static bool command(void *ctx, const hmi_command_t *c, const hmi_protocol_services_t *s)
{
    (void)ctx;
    ++commands_seen;
    hmi_can_frame_t f = {.bus = HMI_BUS_CAN0, .id = 0x600u + c->id, .size = 1u};
    return s->tx->send(s->tx->context, &f);
}
static void adapter_reset(void *ctx)
{
    (void)ctx;
}
static bool route(void *ctx, const hmi_command_t *c, hmi_frame_route_owner_t *owner)
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
static void app_run(uint32_t now, const hmi_command_port_t *commands)
{
    /* 命令同步交给适配器：同一轮里连发两条都应被接受。 */
    if (now == 100u)
    {
        hmi_command_t a = {.id = 1u}, b = {.id = 2u};
        assert(commands->send(commands->context, &a) && commands->send(commands->context, &b));
    }
}
static void app_tick(hmi_snapshot_t *snapshot, uint32_t now)
{
    (void)snapshot;
    (void)now;
    ++ticks;
}
static bool encode(const hmi_snapshot_t *snapshot, uint32_t now, hmi_can_frame_t *frame)
{
    (void)now;
    ++encodes;
    frame->data[0] = (uint8_t)hmi_snapshot_read(snapshot, 1u).value;
    frame->data[1] = counter++;
    return true;
}
static hmi_execution_liveness_t live(void)
{
    hmi_execution_liveness_t l;
    assert(hmi_execution_liveness(&l));
    return l;
}
int main(void)
{
    static const hmi_signal_def_t signal[] = {{.id = 1u, .key = "a", .unit = "", .stale_ms = 0u}};
    static const hmi_catalog_t catalog = {.signals = signal, .signal_count = 1u};
    static const hmi_protocol_adapter_t adapter = {.on_frame = on_frame, .command = command, .reset = adapter_reset};
    static const hmi_protocol_binding_t binding[] = {{.owner = 1u, .adapter = &adapter}};
    static const hmi_protocol_profile_t profile = {binding, 1u};
    static const hmi_frame_route_t entry[] = {{HMI_BUS_CAN0, 0x100u, false, 1u}};
    static const hmi_route_profile_t routes = {entry, 1u};
    static const hmi_periodic_frame_t periodic[] = {
        {.frame = {.bus = HMI_BUS_CAN0, .id = 0x303u, .size = 8u}, .period_ms = 50u, .encode = encode}};
    static const hmi_product_t product = {.catalog = &catalog, .protocols = &profile, .routes = &routes,
        .command_route = route, .periodic = periodic, .periodic_count = 1u, .app_reset = app_reset,
        .app_run = app_run, .app_tick = app_tick};
    static hmi_core_t core;
    static hmi_value_t values[3][1];
    static hmi_diagnostics_t diagnostics;
    hmi_core_storage_t domain = {.signals = values[0], .signal_capacity = 1u};
    hmi_core_storage_t presented = {.signals = values[1], .signal_capacity = 1u};
    hmi_core_storage_t ui = {.signals = values[2], .signal_capacity = 1u};
    assert(hmi_core_init(&core, &catalog, &domain));
    hmi_execution_config_t c = {.product = &product, .core = &core, .published_storage = presented,
                                  .public_diagnostics = &diagnostics};

    /* 启动前不接收动作，看门狗不监督。 */
    hmi_action_t brightness = {.kind = HMI_ACTION_BRIGHTNESS, .value = 30.0f};
    hmi_execution_liveness_t l;
    assert(hmi_execution_start(&c) && !hmi_execution_action(NULL, &brightness) && !hmi_execution_liveness(&l));
    /* 启动：只打开 Product 用到的总线，速率取启动时锁存值；进入第一个会话。 */
    assert(app_start() && opened[HMI_BUS_CAN0] == 1u && opened[HMI_BUS_CAN1] == 0u && opened_bitrate == 250000u);
    assert(resets == 1u && generation != 0u && core.snapshot.connected);
    observed_revision = core.snapshot.revision;
    publish();
    started = true;
    hmi_snapshot_t view = {0};
    hmi_update_view_t update;
    bool normal = false;
    assert(hmi_execution_present(&view, &ui, &update, &normal) && normal);
    assert(!strcmp(update.current_version, HMI_FIRMWARE_DISPLAY_VERSION));

    /* 一轮里：收帧直接写进 Core；周期帧立刻按快照编码入队；同一条目在途时不再排下一帧。 */
    rx_queue[rx_count++] = (hmi_can_frame_t){.bus = HMI_BUS_CAN0, .id = 0x100u, .size = 1u, .data = {42u}};
    fake_now = 10u;
    assert(app_step(fake_now) == 1u && frames_seen == 1u && hmi_snapshot_read(&core.snapshot, 1u).value == 42.0f);
    assert(encodes == 1u && periodic_in_flight[0] && ticks == 1u && changes == 1u);
    assert(hmi_execution_present(&view, &ui, &update, &normal) && hmi_snapshot_read(&view, 1u).value == 42.0f);
    fake_now = 40u;
    (void)app_step(fake_now);
    assert(encodes == 1u);
    assert(tx_service(&tx[HMI_BUS_CAN0]) && sends == 1u && sent[0].id == 0x303u && sent[0].data[0] == 42u);
    assert(!periodic_in_flight[0] && !tx_service(&tx[HMI_BUS_CAN0]));
    fake_now = 60u;
    (void)app_step(fake_now);
    assert(encodes == 2u && periodic_in_flight[0]);
    fake_now = 110u;
    (void)app_step(fake_now);
    assert(encodes == 2u && periodic_deadline[0].missed == 0u);
    /* 超过一个周期还没交给驱动的周期帧作废，不积压。 */
    fake_now = 111u;
    assert(tx_service(&tx[HMI_BUS_CAN0]) && sends == 1u && tx[HMI_BUS_CAN0].dropped == 1u);

    /* 命令端口同步：app_run 一轮连发两条，适配器都接受且帧都入队。 */
    fake_now = 100u;
    periodic_deadline[0].next_ms = 1000u;
    (void)app_step(fake_now);
    assert(commands_seen == 2u);
    assert(tx_service(&tx[HMI_BUS_CAN0]) && tx_service(&tx[HMI_BUS_CAN0]) && sends == 3u);
    assert(sent[1].id == 0x601u && sent[2].id == 0x602u);

    /* 动作队列不是单信用：UI 连发两个动作都能入队，下一轮按序应用。 */
    hmi_action_t units = {.kind = HMI_ACTION_UNITS, .value = 1.0f};
    assert(hmi_execution_action(NULL, &brightness) && hmi_execution_action(NULL, &units));
    fake_now = 200u;
    (void)app_step(fake_now);
    assert(core.snapshot.brightness == 30u && core.snapshot.imperial && actions_rejected == 0u);
    hmi_action_t invalid = {.kind = HMI_ACTION_BRIGHTNESS, .value = 5.0f};
    assert(hmi_execution_action(NULL, &invalid));
    (void)app_step(fake_now);
    assert(actions_rejected == 1u && core.snapshot.brightness == 30u);

    /* 设置尚未加载完成时不接收动作。 */
    nvm_ready = false;
    assert(!hmi_execution_action(NULL, &brightness));
    nvm_ready = true;

    /* 会话切换：旧代次里还没发出的协议帧作废，Product 工作流复位。 */
    hmi_command_t late = {.id = 3u};
    assert(hmi_runtime_command(&runtime, &late));
    restart_session();
    assert(resets == 2u && tx_service(&tx[HMI_BUS_CAN0]) && sends == 3u && tx[HMI_BUS_CAN0].dropped == 2u);
    /* 升级帧不属于会话，代次切换后照常发送。 */
    hmi_can_frame_t uds = {.bus = HMI_BUS_CAN0, .id = 0x7e8u, .size = 8u};
    assert(hmi_execution_can_submit(&uds, true));
    restart_session();
    assert(tx_service(&tx[HMI_BUS_CAN0]) && sends == 4u && sent[3].id == 0x7e8u);
    /* 未打开的总线不接收帧。 */
    uds.bus = HMI_BUS_CAN1;
    assert(!hmi_execution_can_submit(&uds, true));

    /* 看门狗心跳：运行时一轮推进 app_runs，UI 每次发布统计推进 ui_ticks。 */
    uint32_t runs = live().app_runs;
    (void)app_step(fake_now);
    hmi_diagnostics_t ui_diag = {0};
    hmi_execution_ui_diagnostics(&ui_diag);
    assert(live().app_runs == runs + 1u && live().ui_ticks == 1u);

    /* 静默由已有发送 worker 完成：停止排队并撤销普通/升级帧，不影响另一总线。 */
    uds.bus = HMI_BUS_CAN0;
    assert(hmi_execution_can_submit(&uds, true) && hmi_runtime_command(&runtime, &late));
    assert(!runtime_tx.stop(NULL, HMI_BUS_CAN1));
    assert(runtime_tx.stop(NULL, HMI_BUS_CAN0));
    assert(!hmi_execution_can_submit(&uds, true) && !hmi_runtime_command(&runtime, &late));
    stop_ok = false;
    assert(tx_service(&tx[HMI_BUS_CAN0]) && stops[HMI_BUS_CAN0] == 1u && !tx[HMI_BUS_CAN0].stopped);
    stop_ok = true;
    assert(tx_service(&tx[HMI_BUS_CAN0]) && stops[HMI_BUS_CAN0] == 2u && tx[HMI_BUS_CAN0].stopped);
    while (tx_service(&tx[HMI_BUS_CAN0])) {}
    assert(sends == 4u && stops[HMI_BUS_CAN1] == 0u);
    /* 会话切换不恢复硬件参与，必须整机重启。 */
    restart_session();
    assert(!hmi_execution_can_submit(&uds, true));
    return 0;
}
