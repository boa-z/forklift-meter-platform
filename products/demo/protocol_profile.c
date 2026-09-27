#include "products/demo/product.h"
#include "protocols/demo/demo_protocol.h"
static const meter_protocol_binding_t bindings[] = {{1, meter_demo_decode}};
const meter_protocol_profile_t meter_demo_protocols = {bindings, 1};
