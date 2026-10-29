#include "core/hmi_core.h"
#include "runtime/hmi_runtime.h"
#include <stdio.h>
#include "check.h"
typedef struct
{
    unsigned frames, processes, commands, resets, sends;
    void *update_seen;
} state_t;
static bool on_frame(void *ctx, const hmi_can_frame_t *f, const hmi_protocol_services_t *s)
{
    state_t *a = ctx;
    ++a->frames;
    a->update_seen = s->update_context;
    hmi_update_t u = {42, {17, f->timestamp_ms, HMI_VALUE_VALID, 7}};
    return s->update(s->update_context, &u);
}
static bool process(void *ctx, uint32_t now, const hmi_protocol_services_t *s)
{
    ++((state_t *)ctx)->processes;
    hmi_update_t u = {42, {63, now, HMI_VALUE_VALID, 7}};
    return s->update(s->update_context, &u);
}
static bool send(void *ctx, const hmi_can_frame_t *f)
{
    ++((state_t *)ctx)->sends;
    return f->id == 0x123;
}
static bool command(void *ctx, const hmi_command_t *cmd, const hmi_protocol_services_t *s)
{
    ++((state_t *)ctx)->commands;
    // Adapter 只能经 services->tx 发送；router 已被禁止直接拿 tx。
    if (!s->tx || !s->tx->send)
        return false;
    hmi_can_frame_t f = {.bus = HMI_BUS_CAN0, .id = cmd->id, .size = 1};
    return s->tx->send(s->tx->context, &f);
}
static void reset(void *ctx) { ++((state_t *)ctx)->resets; }
static bool route_ok(void *ctx, const hmi_command_t *cmd, hmi_frame_route_owner_t *owner)
{
    (void)ctx;
    if (cmd->id != 0x123)
        return false;
    *owner = 1;
    return true;
}
static bool route_reject(void *ctx, const hmi_command_t *cmd, hmi_frame_route_owner_t *owner)
{
    (void)ctx;
    (void)cmd;
    (void)owner;
    return false;
}
int main(void)
{
    state_t a = {0};
    hmi_signal_def_t signal = {.id = 42, .key = "test.speed", .unit = "km/h", .stale_ms = 500};
    hmi_catalog_t catalog = {.signals = &signal, .signal_count = 1};
    hmi_value_t value;
    hmi_core_storage_t storage = {.signals = &value, .signal_capacity = 1};
    hmi_core_t core;
    CHECK(hmi_core_init(&core, &catalog, &storage));
    hmi_protocol_adapter_t adapter = {&a, on_frame, process, command, reset};
    hmi_protocol_binding_t binding = {1, NULL, &adapter};
    hmi_protocol_profile_t protocols = {&binding, 1};
    hmi_frame_route_t route = {HMI_BUS_CAN0, 0x123, false, 1};
    hmi_route_profile_t routes = {&route, 1};
    hmi_product_t product = {.id = "adapter-test",
                               .protocols = &protocols,
                               .routes = &routes,
                               .catalog = &catalog,
                               .command_route = route_ok,
                               .command_route_context = &a};
    hmi_runtime_t runtime;
    CHECK(hmi_runtime_init(&runtime, &product, hmi_core_apply, &core));
    hmi_can_tx_port_t tx = {.send = send, .context = &a};
    hmi_runtime_bind_tx(&runtime, &tx);
    hmi_command_t cmd = {0x123, 0, 0};
    CHECK(!hmi_runtime_process(&runtime, 10));
    CHECK(!hmi_runtime_command(&runtime, &cmd));
    hmi_runtime_connection(&runtime, true);
    hmi_can_frame_t f = {.bus = HMI_BUS_CAN0, .id = 0x123, .size = 1};
    CHECK(hmi_runtime_receive(&runtime, &f));
    CHECK(a.frames == 1 && a.update_seen == &core);
    CHECK(value.value == 17 && value.source == 7);
    CHECK(hmi_runtime_process(&runtime, 123));
    CHECK(a.processes == 1 && value.timestamp_ms == 123 && value.value == 63);
    CHECK(hmi_runtime_command(&runtime, &cmd));
    CHECK(a.commands == 1 && a.sends == 1);
    /* App 用的命令端口与直接调用等价。 */
    const hmi_command_port_t port = hmi_runtime_command_port(&runtime);
    CHECK(port.send(port.context, &cmd) && a.commands == 2 && a.sends == 2);
    cmd.id = 0x124;
    CHECK(!hmi_runtime_command(&runtime, &cmd));
    product.command_route = route_reject;
    cmd.id = 0x123;
    CHECK(!hmi_runtime_command(&runtime, &cmd));
    product.command_route = route_ok;
    // 未绑定 TX 时 adapter 看到 NULL，自己拒绝发送。
    hmi_runtime_bind_tx(&runtime, NULL);
    CHECK(!hmi_runtime_command(&runtime, &cmd));
    hmi_runtime_bind_tx(&runtime, &tx);
    hmi_runtime_connection(&runtime, false);
    hmi_runtime_connection(&runtime, false);
    CHECK(a.resets == 1 && !hmi_runtime_receive(&runtime, &f) && a.frames == 1);
    CHECK(!hmi_runtime_command(&runtime, &cmd));
    hmi_runtime_connection(&runtime, true);
    /* 未路由的帧计数后丢弃。 */
    f.id = 0x124;
    CHECK(!hmi_runtime_receive(&runtime, &f) && runtime.diagnostics.unrouted == 1);
    adapter.on_frame = NULL;
    CHECK(!hmi_runtime_init(&runtime, &product, hmi_core_apply, &core));
    return 0;
}
