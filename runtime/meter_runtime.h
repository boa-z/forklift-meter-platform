#ifndef METER_RUNTIME_H
#define METER_RUNTIME_H
#include "contracts/meter_product.h"
#define METER_RX_CAPACITY 32u
typedef struct
{
    uint32_t accepted, overflow, malformed, unrouted, decode_failed, dispatched;
} meter_runtime_diagnostics_t;
typedef struct
{
    meter_can_frame_t frame;
    uint32_t generation;
} meter_queued_frame_t;
/* Single protocol-context owner. ISR producers must use the platform's native
 * IPC. */
typedef struct
{
    const meter_product_t *product;
    meter_update_sink_t sink;
    void *context;
    meter_queued_frame_t queue[METER_RX_CAPACITY];
    size_t head, tail, count;
    uint32_t generation;
    bool connected;
    meter_runtime_diagnostics_t diagnostics;
} meter_runtime_t;
bool meter_runtime_init(meter_runtime_t *runtime, const meter_product_t *product, meter_update_sink_t sink,
                        void *context);
void meter_runtime_connection(meter_runtime_t *runtime, bool connected);
bool meter_runtime_push(meter_runtime_t *runtime, const meter_can_frame_t *frame);
size_t meter_runtime_poll(meter_runtime_t *runtime, size_t budget);
/** @brief 协议线程串行驱动周期任务；时间单位 ms，断开时返回 false。 */
bool meter_runtime_process(meter_runtime_t *runtime, uint32_t now_ms);
/** @brief 同步路由车辆命令；端口仅在回调期间有效，断开或无发送端口时拒绝。 */
bool meter_runtime_command(meter_runtime_t *runtime, meter_frame_route_owner_t owner,
                           const meter_command_t *command, const meter_can_tx_port_t *tx);
#endif
