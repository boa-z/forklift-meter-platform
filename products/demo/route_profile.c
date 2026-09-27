#include "products/demo/product.h"
static const meter_frame_route_t entries[] = {
#include "products/demo/generated/can/routes.inc"
};
const meter_route_profile_t meter_demo_routes = {entries, sizeof(entries) / sizeof(entries[0])};
