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
static const meter_product_t product = {
    "reference-demo",    &meter_demo_capabilities, &meter_demo_protocols, &meter_demo_routes,
    PRODUCT_UI,          &meter_demo_resources,    &meter_demo_locale,    &meter_demo_auth,
    &meter_demo_catalog, meter_demo_evaluate,      demo_command_route,    NULL,
    &storage_profile,    &update_policy,           update_admission};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
