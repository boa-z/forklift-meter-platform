#ifndef METER_FRAME_ROUTER_H
#define METER_FRAME_ROUTER_H
#include "contracts/meter_product.h"
bool meter_frame_valid(const meter_can_frame_t *frame);
bool meter_routes_valid(const meter_route_profile_t *routes);
const meter_frame_route_t *meter_route_lookup(const meter_route_profile_t *routes,
                                              const meter_can_frame_t *frame);
#endif
