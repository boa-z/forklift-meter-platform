#include "product/product.h"
#include "generated/demo_catalog.h"
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
static const meter_product_t product = {
    "reference-demo",    &meter_demo_capabilities, &meter_demo_protocols, &meter_demo_routes,
    PRODUCT_UI,          &meter_demo_resources,    &meter_demo_locale,    &meter_demo_auth,
    &meter_demo_catalog, meter_demo_evaluate,      demo_command_route,    NULL,
    &storage_profile};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
