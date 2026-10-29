#ifndef HMI_FRAME_ROUTER_H
#define HMI_FRAME_ROUTER_H
#include "contracts/hmi_product.h"
bool hmi_frame_valid(const hmi_can_frame_t *frame);
bool hmi_routes_valid(const hmi_route_profile_t *routes);
const hmi_frame_route_t *hmi_route_lookup(const hmi_route_profile_t *routes,
                                              const hmi_can_frame_t *frame);
#endif
