#ifndef HMI_PRODUCT_HOST_H
#define HMI_PRODUCT_HOST_H
#include "runtime/hmi_runtime.h"
#include "core/hmi_core.h"
/** @brief Host 本机动作入口；Product 动作沿用板端回调。 */
static inline bool hmi_product_host_action(hmi_core_t *core, const hmi_product_t *product,
                                             const hmi_action_t *request, uint32_t now_ms)
{
    if (!core || !product || !request)
        return false;
    return request->kind == HMI_ACTION_PRODUCT
        ? product->local_action && product->local_action(&core->snapshot, request, now_ms)
        : hmi_core_action(core, request);
}
/** @brief Host 通用生命周期；step 只投递合成帧（hmi_runtime_receive），全部回调在唯一线程串行执行。 */
int hmi_product_host(int argc, char **argv, void (*step)(hmi_runtime_t *, uint32_t));
#endif
