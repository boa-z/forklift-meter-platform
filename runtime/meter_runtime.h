#ifndef METER_RUNTIME_H
#define METER_RUNTIME_H
#include "contracts/meter_product.h"
#include "diagnostics/meter_diagnostics.h"
#include <stddef.h>
#define METER_RX_CAPACITY 32u
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
    meter_diagnostics_t *diag;
    meter_update_sink_t update;
    void *update_context;
    meter_queued_frame_t queue[METER_RX_CAPACITY];
    size_t head, tail, count;
    uint32_t generation;
    bool connected;
    meter_protocol_event_sink_t event_sink;
    void *event_context;
    const meter_can_tx_port_t *tx;
    meter_runtime_diagnostics_t diagnostics;
} meter_runtime_t;
bool meter_runtime_init(meter_runtime_t *runtime, const meter_product_t *product, meter_update_sink_t update,
                        void *update_context);
/** @brief 绑定瞬时事件与发送服务；update 沿用 init 的 Domain 绑定；对象由调用方保证生命周期。 */
void meter_runtime_bind_services(meter_runtime_t *runtime, meter_protocol_event_sink_t event_sink,
                                 void *event_context, const meter_can_tx_port_t *tx);
void meter_runtime_connection(meter_runtime_t *runtime, bool connected);
bool meter_runtime_push(meter_runtime_t *runtime, const meter_can_frame_t *frame);
size_t meter_runtime_poll(meter_runtime_t *runtime, size_t budget);
/** @brief 协议线程串行驱动周期任务；时间单位 ms，断开时返回 false。 */
bool meter_runtime_process(meter_runtime_t *runtime, uint32_t now_ms);
/** @brief 由 Product 路由业务命令并统一调用目标 Adapter；Application 不选择 owner、不提供 TX。
 *
 * Router 只返回目标 owner，实际发送只能由 Adapter 经 services->tx 完成。
 * 未绑定 TX、未连接、无 router 或目标无 command 实现时返回 false。
 */
bool meter_runtime_command(meter_runtime_t *runtime, const meter_command_t *command);
/** @brief 绑定可选诊断实例；实例必须覆盖 Runtime 生命周期，调用在 owner 线程。 */
void meter_runtime_bind_diagnostics(meter_runtime_t *runtime, meter_diagnostics_t *diag);
#endif
