#include "products/demo/product.h"
static const meter_frame_route_t entries[] = {{METER_BUS_CAN0, 0x100, false, 1},
                                              {METER_BUS_CAN0, 0x101, false, 1},
                                              {METER_BUS_CAN0, 0x102, false, 1},
                                              {METER_BUS_CAN0, 0x103, false, 1},
                                              {METER_BUS_CAN0, 0x104, false, 1}};
const meter_route_profile_t meter_demo_routes = {entries, sizeof(entries) / sizeof(entries[0])};
