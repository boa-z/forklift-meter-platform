#ifndef HMI_SETTINGS_H
#define HMI_SETTINGS_H
#include "core/hmi_core.h"
/** @brief MSP3：12 字节头含速率选择、稳定 ID 参数项与 4 字节校验；不迁移旧格式。 */
#define HMI_SETTINGS_OVERHEAD 16u
#define HMI_SETTINGS_ENTRY_SIZE 8u
/** @brief 返回产品完整偏好记录的编码容量；不支持的浮点表示或参数数返回零。 */
size_t hmi_settings_size(const hmi_core_t *core);
/** @brief App 把本机权威设置编码为不可变副本；不进行 I/O，不写原生 struct。 */
bool hmi_settings_encode(const hmi_core_t *core, uint8_t *out, size_t size);
/** @brief App 先验证所有稳定 ID、重复项和范围，再整体应用并更新实际变化的 revision。 */
bool hmi_settings_decode(hmi_core_t *core, const uint8_t *data, size_t size);
/** @brief Product 显式选择只增参数兼容；缺项使用目录 initial，其余校验及原子应用不变。 */
bool hmi_settings_decode_append_only(hmi_core_t *core, const uint8_t *data, size_t size);
#endif
