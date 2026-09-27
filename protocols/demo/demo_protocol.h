#ifndef DEMO_PROTOCOL_H
#define DEMO_PROTOCOL_H
#include "contracts/meter_product.h"
/* 仅用于演示的合成帧格式，与任何量产车辆协议都不兼容；帧布局与单位见 demo_protocol.c。 */
bool meter_demo_decode(const meter_can_frame_t *frame, meter_update_sink_t sink, void *context);
#endif
