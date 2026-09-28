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
/* 仅公开合成台架使用；真实车辆 Product 必须定义自己的报文与安全策略。 */
static const meter_periodic_frame_t periodic[] = {
    {.frame = {.bus = METER_BUS_CAN0, .id = 0x3c0u, .size = 2u, .data = {0x50u, 0x01u}}, .period_ms = 50u, .critical = true},
    {.frame = {.bus = METER_BUS_CAN0, .id = 0x2f0u, .size = 2u, .data = {0x64u, 0x01u}}, .period_ms = 100u, .critical = false}
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
    .periodic = periodic, .periodic_count = sizeof(periodic) / sizeof(periodic[0])};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
