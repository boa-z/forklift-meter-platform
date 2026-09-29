#include "services/settings_app.h"
#include "product/product.h"
#include "generated/demo_catalog.h"
#include "update/meter_update.h"
#ifdef METER_HEADLESS
#define PRODUCT_UI NULL
#else
#define PRODUCT_UI &meter_demo_ui
#endif
/** @brief Demo 无车辆控制命令；router 仅证明新签名（只选 owner，不触碰 TX）。 */
static bool demo_command_route(void *context, const meter_command_t *command,
                               meter_frame_route_owner_t *owner_out)
{
    (void)context;
    (void)command;
    (void)owner_out;
    return false;
}
/* 公开合成 Demo 参数为本机权威；真实远端参数由其 Product 排除持久化。 */
static const meter_storage_profile_t storage_profile = {true, 1u, 0x444Du, 2u, 500u, 3000u};
/* 公开合成台架仅由本机维护开关准入；真实 Product 必须增加驻车/车速策略。 */
static bool update_admission(const meter_snapshot_t *snapshot, bool maintenance)
{
    (void)snapshot;
    return maintenance;
}
static const meter_update_policy_t update_policy = {"reference-demo", "reference-board",
                                                    4u * 1024u * 1024u + 2048u, 15000u};
/** @brief App 采样本机亮度和车速；保留车速来源时间，重复发布不伪造新样本。 */
static bool tx_sample(const meter_snapshot_t *snapshot, uint32_t now, meter_tx_value_t *values, size_t count)
{
    if (!snapshot || !values || count != 2u) return false;
    meter_value_t speed = meter_snapshot_read(snapshot, METER_SPEED);
    bool valid = speed.state == METER_VALUE_VALID && speed.value >= 0.0f && speed.value <= 655.0f;
    values[0] = (meter_tx_value_t){.value = snapshot->brightness, .sample_ms = now, .valid = true};
    values[1] = (meter_tx_value_t){.value = valid ? (int32_t)(speed.value * 100.0f) : 0,
        .sample_ms = speed.timestamp_ms, .valid = valid};
    return true;
}
/** @brief 公开合成协议：数值、fresh、计数、revision、反码及异或校验，均由 Protocol 编码。 */
static bool tx_encode(const meter_tx_snapshot_t *snapshot, bool fresh, uint32_t wire,
                      meter_can_frame_t *frame, uint32_t *next_wire)
{
    if (!snapshot || snapshot->count != 2u || !snapshot->values || !frame || !next_wire) return false;
    size_t index = frame->id == 0x3c0u ? 0u : 1u;
    uint16_t value = (uint16_t)snapshot->values[index].value;
    frame->size = 8u;
    frame->data[0] = (uint8_t)value; frame->data[1] = (uint8_t)(value >> 8);
    frame->data[2] = fresh ? 1u : 0u; frame->data[3] = (uint8_t)wire;
    frame->data[4] = (uint8_t)snapshot->revision; frame->data[5] = (uint8_t)(snapshot->revision >> 8);
    frame->data[6] = (uint8_t)~frame->data[0]; frame->data[7] = 0u;
    for (unsigned i = 0u; i < 7u; ++i) frame->data[7] ^= frame->data[i];
    *next_wire = (wire + 1u) & 0xffu;
    return true;
}
static const meter_periodic_frame_t periodic[] = {
    {.frame = {.bus = METER_BUS_CAN0, .id = 0x3c0u}, .period_ms = 50u, .critical = true,
     .encode = tx_encode, .first_value = 0u, .value_count = 1u, .max_age_ms = 200u,
     .freshness = METER_TX_ENCODE_INVALID, .backlog = METER_TX_REPLACE_PENDING, .commit = METER_TX_COMMIT_DRIVER},
    {.frame = {.bus = METER_BUS_CAN0, .id = 0x2f0u}, .period_ms = 100u,
     .encode = tx_encode, .first_value = 1u, .value_count = 1u, .max_age_ms = 500u,
     .freshness = METER_TX_ENCODE_INVALID, .commit = METER_TX_COMMIT_ADMISSION}
};
/** @brief 维护时保留关键周期帧，拒绝普通命令/设置，展示升级视图。 */
static meter_mode_policy_t mode_policy(meter_mode_t mode)
{
    meter_mode_policy_t p = {0};
    if (mode == METER_MODE_NORMAL) p = (meter_mode_policy_t){true, true, true, true, true, true};
    else if (mode == METER_MODE_DEGRADED) p = (meter_mode_policy_t){true, false, true, false, false, true};
    else if (mode == METER_MODE_UPDATE_MAINTENANCE) p.critical_tx = true;
    return p;
}
static const meter_product_t product = {
    .id = "reference-demo", .capabilities = &meter_demo_capabilities, .protocols = &meter_demo_protocols,
    .routes = &meter_demo_routes, .ui = PRODUCT_UI, .resources = &meter_demo_resources,
    .locale = &meter_demo_locale, .auth = &meter_demo_auth, .catalog = &meter_demo_catalog,
    .evaluate = meter_demo_evaluate, .command_route = demo_command_route,
    .storage = &storage_profile, .update = &update_policy, .update_admission = update_admission,
    .update_exclusive = true, .mode_policy = mode_policy,
    .tx_sample = tx_sample, .tx_value_count = 2u,
    .local_action = demo_settings_action, .app_reset = demo_settings_reset, .app_run = demo_settings_run,
    .periodic = periodic, .periodic_count = sizeof(periodic) / sizeof(periodic[0])};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
