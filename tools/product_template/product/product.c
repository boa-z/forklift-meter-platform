#include "contracts/hmi_product.h"
#include "catalog/catalog.h"
#include "i18n/generated/product_i18n.h"
#ifndef HMI_HEADLESS
extern const hmi_ui_factory_t product_ui;
#define UI_FACTORY &product_ui
#else
#define UI_FACTORY NULL
#endif
extern bool product_decode(const hmi_can_frame_t *,hmi_update_sink_t,void *);
static bool product_command_route(void *context, const hmi_command_t *command,
                                  hmi_frame_route_owner_t *owner)
{
    (void)context;
    (void)command;
    (void)owner;
    return false;
}
static const hmi_protocol_binding_t bindings[]={{1,product_decode,NULL}};
static const hmi_protocol_profile_t protocols={bindings,1};
static const hmi_frame_route_t entries[]={
#include "generated/can/routes.inc"
};
static const hmi_route_profile_t routes={entries,sizeof(entries)/sizeof(entries[0])};
/* 最小 Product：一路 DBC 解码与 UI。按需再加 periodic、app_run/app_tick、local_action、storage、update。 */
static const hmi_initial_settings_t initial_settings = {
    .language = PRODUCT_I18N_DEFAULT_LANGUAGE, .brightness = 100, .can_rate = HMI_CAN_RATE_125K};
static const hmi_product_t product={.id="@PRODUCT_ID@",.protocols=&protocols,.routes=&routes,.catalog=&product_catalog,
    .ui=UI_FACTORY,.initial_settings=&initial_settings,.command_route=product_command_route,.command_route_context=NULL};
/** @brief 返回静态产品定义；实例及其所引用对象在进程生命周期内有效。 */
const hmi_product_t *hmi_product_get(void) { return &product; }
