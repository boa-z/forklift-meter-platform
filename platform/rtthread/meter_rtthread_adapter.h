#ifndef METER_RTTHREAD_ADAPTER_H
#define METER_RTTHREAD_ADAPTER_H
#include "core/meter_core.h"
#include "runtime/meter_runtime.h"
typedef struct
{
    bool (*display_init)(void *context);
    bool (*touch_init)(void *context);
    bool (*can_open)(void *context, meter_bus_role_t bus);
    bool (*can_read)(void *context, meter_can_frame_t *frame);
    bool (*load_settings)(void *context, uint8_t *data, size_t capacity, size_t *size);
    bool (*save_settings)(void *context, const uint8_t *data, size_t size);
    void *context;
    /* Monotonic clock must advance even when no CAN frame arrives. */
    uint32_t (*now_ms)(void *context);
} meter_rtthread_board_port_t;
typedef struct
{
    meter_runtime_t runtime;
    meter_core_t *core;
    meter_rtthread_board_port_t board;
} meter_rtthread_adapter_t;
bool meter_rtthread_adapter_init(meter_rtthread_adapter_t *adapter, const meter_product_t *product,
                                 meter_core_t *core, const meter_rtthread_board_port_t *board);
bool meter_rtthread_adapter_poll(meter_rtthread_adapter_t *adapter, size_t budget);
void meter_rtthread_adapter_disconnect(meter_rtthread_adapter_t *adapter);
const meter_runtime_diagnostics_t *
meter_rtthread_adapter_diagnostics(const meter_rtthread_adapter_t *adapter);
#endif
