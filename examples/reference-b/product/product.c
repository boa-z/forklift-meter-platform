#include "contracts/meter_product.h"
#include "catalog/catalog.h"
#ifndef METER_HEADLESS
extern const meter_ui_factory_t product_ui;
#define UI_FACTORY &product_ui
#else
#define UI_FACTORY NULL
#endif
extern bool product_decode(const meter_can_frame_t *,meter_update_sink_t,void *);
static const meter_protocol_binding_t bindings[]={{1,product_decode,NULL}};
static const meter_protocol_profile_t protocols={bindings,1};
static const meter_frame_route_t entries[]={
#include "generated/can/routes.inc"
};
static const meter_route_profile_t routes={entries,sizeof(entries)/sizeof(entries[0])};
static const meter_capability_profile_t capabilities={.language_selection=true};
static const meter_locale_profile_t locale={"en","reference-b"};
static const meter_resource_profile_t resources={"assets/README.md"};
static const meter_auth_profile_t auth={true,false};
static const meter_product_t product={.id="reference-b",.protocols=&protocols,.routes=&routes,.catalog=&product_catalog,
    .ui=UI_FACTORY,.capabilities=&capabilities,.locale=&locale,.resources=&resources,.auth=&auth};
/** @brief 返回静态产品定义；实例及其所引用对象在进程生命周期内有效。 */
const meter_product_t *meter_product_get(void) { return &product; }
