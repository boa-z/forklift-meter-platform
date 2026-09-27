#ifndef DEMO_PRODUCT_H
#define DEMO_PRODUCT_H
#include "contracts/meter_product.h"
extern const meter_capability_profile_t meter_demo_capabilities;
extern const meter_protocol_profile_t meter_demo_protocols;
extern const meter_route_profile_t meter_demo_routes;
extern const meter_ui_factory_t meter_demo_ui;
extern const meter_resource_profile_t meter_demo_resources;
extern const meter_locale_profile_t meter_demo_locale;
extern const meter_auth_profile_t meter_demo_auth;
void meter_demo_evaluate(meter_snapshot_t *snapshot);
#endif
