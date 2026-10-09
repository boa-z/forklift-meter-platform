#ifndef METER_PRODUCT_HOST_H
#define METER_PRODUCT_HOST_H
#include "runtime/meter_runtime.h"
#include "core/meter_core.h"
/** @brief Host 本机动作入口；Product 动作沿用板端回调。 */
static inline bool meter_product_host_action(meter_core_t *core, const meter_product_t *product,
                                             const meter_action_t *request, uint32_t now_ms)
{
    if (!core || !product || !request)
        return false;
    return request->kind == METER_ACTION_PRODUCT
        ? product->local_action && product->local_action(&core->snapshot, request, now_ms)
        : meter_core_action(core, request);
}
/** @brief Host 通用生命周期；step 只投递合成帧，全部回调在唯一 UI 线程串行执行。 */
int meter_product_host(int argc, char **argv, void (*step)(meter_runtime_t *, uint32_t));
#endif
