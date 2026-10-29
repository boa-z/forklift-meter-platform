#ifndef HMI_RUNTIME_H
#define HMI_RUNTIME_H
#include "contracts/hmi_product.h"
#include "core/hmi_core.h"
#include "diagnostics/hmi_diagnostics.h"
#include <stddef.h>
/**
 * @brief 协议运行时：按 Product 路由把收到的帧交给 decode/adapter，驱动 adapter 周期任务与命令。
 *
 * 单一 owner：固件里是运行时线程，主机上是模拟器主循环。不持有队列，收到即分发；
 * 解码结果经 update sink 直接写入 Core。
 */
typedef struct
{
    const hmi_product_t *product;
    hmi_diagnostics_t *diag;
    hmi_update_sink_t update;
    void *update_context;
    const hmi_can_tx_port_t *tx;
    uint32_t generation;
    bool connected;
    hmi_runtime_diagnostics_t diagnostics;
} hmi_runtime_t;
/** @brief 校验 Product 的路由与绑定并绑定 Domain 写入口；失败不改 runtime。 */
bool hmi_runtime_init(hmi_runtime_t *runtime, const hmi_product_t *product, hmi_update_sink_t update,
                        void *update_context);
/** @brief 绑定发送端口；端口须覆盖运行期，未绑定时 adapter 看到的 tx 为 NULL。 */
void hmi_runtime_bind_tx(hmi_runtime_t *runtime, const hmi_can_tx_port_t *tx);
/** @brief 绑定可选诊断实例；实例须覆盖运行期。 */
void hmi_runtime_bind_diagnostics(hmi_runtime_t *runtime, hmi_diagnostics_t *diag);
/** @brief 连接状态变化推进代次；断开时复位全部 adapter。 */
void hmi_runtime_connection(hmi_runtime_t *runtime, bool connected);
/** @brief 收到一帧：校验、路由并立即交给目标 owner。未连接、畸形、未路由或解码失败返回 false。 */
bool hmi_runtime_receive(hmi_runtime_t *runtime, const hmi_can_frame_t *frame);
/** @brief 每轮驱动 adapter 的 process；未连接返回 false。 */
bool hmi_runtime_process(hmi_runtime_t *runtime, uint32_t now_ms);
/** @brief 按 Product 的 command_route 交给目标 adapter 的 command()；返回 adapter 是否接受。 */
bool hmi_runtime_command(hmi_runtime_t *runtime, const hmi_command_t *command);
/** @brief 本 runtime 的命令端口，交给 Product 的 app_run 使用。 */
hmi_command_port_t hmi_runtime_command_port(hmi_runtime_t *runtime);
/**
 * @brief 运行时线程一轮的 Product 业务，固件与主机共用同一顺序：
 * 协议周期任务 → app_run（commands 为 false 时两者都跳过）→ 信号超时 → app_tick。
 */
void hmi_runtime_step(hmi_runtime_t *runtime, hmi_core_t *core, uint32_t now_ms, bool commands);
#endif
