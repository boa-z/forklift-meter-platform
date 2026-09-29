#include "product/product.h"
#include "generated/demo_catalog.h"
#include "protocol/can/demo_pdo.h"
#include "services/settings_app.h"
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
static const meter_storage_profile_t storage_profile = {true, 1u, 0x444Du, 3u, 500u, 3000u};
/* 公开合成台架仅由本机维护开关准入；真实 Product 必须增加驻车/车速策略。 */
static bool update_admission(const meter_snapshot_t *snapshot, bool maintenance)
{
    (void)snapshot;
    return maintenance;
}
static const meter_update_policy_t update_policy = {"reference-demo", "reference-board",
                                                    4u * 1024u * 1024u + 2048u, 15000u};
/** @brief 保留源时间；范围检查先于浮点转整数，拒绝非有限数和未知/过期值。 */
static meter_tx_value_t sample_measurement(const meter_snapshot_t *snapshot, meter_signal_id_t id,
                                           float maximum, float scale)
{
    meter_value_t value = meter_snapshot_read(snapshot, id);
    bool valid = value.state == METER_VALUE_VALID && value.value >= 0.0f && value.value <= maximum;
    return (meter_tx_value_t){
        .value = valid ? (int32_t)(value.value * scale) : 0, .sample_ms = value.timestamp_ms, .valid = valid};
}
/** @brief App 发布工程量整数；编码及逐帧新鲜度判定仍归 Protocol 所有。 */
static bool tx_sample(const meter_snapshot_t *snapshot, uint32_t now, meter_tx_value_t *values, size_t count)
{
    if (!snapshot || !values || count != DEMO_TX_VALUE_COUNT)
        return false;
    meter_value_t speed = meter_snapshot_read(snapshot, METER_SPEED);
    bool valid = speed.state == METER_VALUE_VALID && speed.value >= 0.0f && speed.value <= 655.0f;
    values[0] = (meter_tx_value_t){.value = snapshot->brightness, .sample_ms = now, .valid = true};
    values[1] = (meter_tx_value_t){.value = valid ? (int32_t)(speed.value * 100.0f) : 0,
                                   .sample_ms = speed.timestamp_ms,
                                   .valid = valid};
    values[DEMO_TX_SOC] = sample_measurement(snapshot, METER_SOC, 100.0f, 1.0f);
    values[DEMO_TX_HEIGHT] = sample_measurement(snapshot, METER_HEIGHT, 6.0f, 1000.0f);
    values[DEMO_TX_LOAD] = sample_measurement(snapshot, METER_LOAD, 1500.0f, 1.0f);
    const meter_signal_id_t flags[] = {METER_SEAT, METER_BRAKE, METER_NEUTRAL, METER_CHARGING, METER_WARNING};
    for (unsigned i = 0u; i < sizeof(flags) / sizeof(flags[0]); ++i)
    {
        meter_value_t flag = meter_snapshot_read(snapshot, flags[i]);
        bool known = flag.state == METER_VALUE_VALID && (flag.value == 0.0f || flag.value == 1.0f);
        values[DEMO_TX_SEAT + i] = (meter_tx_value_t){
            .value = known ? (int32_t)flag.value : 0, .sample_ms = flag.timestamp_ms, .valid = known};
    }
    return true;
}
/** @brief 公开合成协议：数值、fresh、计数、revision、反码及异或校验，均由 Protocol 编码。 */
static bool tx_encode(const meter_tx_snapshot_t *snapshot, bool fresh, uint32_t wire,
                      meter_can_frame_t *frame, uint32_t *next_wire)
{
    if (!snapshot || snapshot->count != DEMO_TX_VALUE_COUNT || !snapshot->values || !frame || !next_wire)
        return false;
    size_t index = frame->id == 0x3c0u ? 0u : 1u;
    uint16_t value = (uint16_t)snapshot->values[index].value;
    frame->size = 8u;
    frame->data[0] = (uint8_t)value;
    frame->data[1] = (uint8_t)(value >> 8);
    frame->data[2] = fresh ? 1u : 0u;
    frame->data[3] = (uint8_t)wire;
    frame->data[4] = (uint8_t)snapshot->revision;
    frame->data[5] = (uint8_t)(snapshot->revision >> 8);
    frame->data[6] = (uint8_t)~frame->data[0];
    frame->data[7] = 0u;
    for (unsigned i = 0u; i < 7u; ++i)
        frame->data[7] ^= frame->data[i];
    *next_wire = (wire + 1u) & 0xffu;
    return true;
}
static const meter_periodic_frame_t periodic[] = {{.frame = {.bus = METER_BUS_CAN0, .id = 0x3c0u},
                                                   .period_ms = 50u,
                                                   .critical = true,
                                                   .encode = tx_encode,
                                                   .first_value = 0u,
                                                   .value_count = 1u,
                                                   .max_age_ms = 200u,
                                                   .freshness = METER_TX_ENCODE_INVALID,
                                                   .backlog = METER_TX_REPLACE_PENDING,
                                                   .commit = METER_TX_COMMIT_DRIVER},
                                                  {.frame = {.bus = METER_BUS_CAN0, .id = 0x2f0u},
                                                   .period_ms = 100u,
                                                   .encode = tx_encode,
                                                   .first_value = 1u,
                                                   .value_count = 1u,
                                                   .max_age_ms = 500u,
                                                   .freshness = METER_TX_ENCODE_INVALID,
                                                   .commit = METER_TX_COMMIT_ADMISSION},
                                                  {.frame = {.bus = METER_BUS_CAN0, .id = DEMO_PDO_MOTION_ID},
                                                   .period_ms = 100u,
                                                   .encode = demo_pdo_encode,
                                                   .first_value = DEMO_TX_SPEED,
                                                   .value_count = 4u,
                                                   .max_age_ms = 500u,
                                                   .freshness = METER_TX_ENCODE_INVALID,
                                                   .commit = METER_TX_COMMIT_DRIVER},
                                                  {.frame = {.bus = METER_BUS_CAN0, .id = DEMO_PDO_STATUS_ID},
                                                   .period_ms = 100u,
                                                   .encode = demo_pdo_encode,
                                                   .first_value = DEMO_TX_SEAT,
                                                   .value_count = 5u,
                                                   .max_age_ms = 500u,
                                                   .freshness = METER_TX_ENCODE_INVALID,
                                                   .commit = METER_TX_COMMIT_DRIVER}};
/** @brief 维护时保留关键周期帧，拒绝普通命令/设置，展示升级视图。 */
static meter_mode_policy_t mode_policy(meter_mode_t mode)
{
    meter_mode_policy_t p = {0};
    if (mode == METER_MODE_NORMAL)
        p = (meter_mode_policy_t){true, true, true, true, true, true};
    else if (mode == METER_MODE_DEGRADED)
        p = (meter_mode_policy_t){true, false, true, false, false, true};
    else if (mode == METER_MODE_UPDATE_MAINTENANCE)
        p.critical_tx = true;
    return p;
}
static const meter_product_t product = {.id = "reference-demo",
                                        .capabilities = &meter_demo_capabilities,
                                        .protocols = &meter_demo_protocols,
                                        .routes = &meter_demo_routes,
                                        .ui = PRODUCT_UI,
                                        .resources = &meter_demo_resources,
                                        .locale = &meter_demo_locale,
                                        .auth = &meter_demo_auth,
                                        .catalog = &meter_demo_catalog,
                                        .evaluate = meter_demo_evaluate,
                                        .command_route = demo_command_route,
                                        .storage = &storage_profile,
                                        .update = &update_policy,
                                        .update_admission = update_admission,
                                        .update_exclusive = true,
                                        .mode_policy = mode_policy,
                                        .tx_sample = tx_sample,
                                        .tx_value_count = DEMO_TX_VALUE_COUNT,
                                        .local_action = demo_settings_action,
                                        .app_reset = demo_settings_reset,
                                        .app_run = demo_settings_run,
                                        .periodic = periodic,
                                        .periodic_count = sizeof(periodic) / sizeof(periodic[0])};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
