#ifndef DEMO_PROTOCOL_H
#define DEMO_PROTOCOL_H
#include "contracts/meter_product.h"
/* FOR DEMONSTRATION ONLY. NOT COMPATIBLE WITH PRODUCTION VEHICLES. */
bool meter_demo_decode(const meter_can_frame_t *frame, meter_update_sink_t sink, void *context);
#endif
