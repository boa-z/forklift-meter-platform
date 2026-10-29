#ifndef HMI_HOST_SETTINGS_H
#define HMI_HOST_SETTINGS_H
#include "core/hmi_settings.h"
#include "contracts/hmi_product.h"
#include "storage/hmi_nvm.h"
/** @brief Host 设置会话仅在启动分配固定容量；后台线程独占文件后端。 */
typedef struct hmi_host_nvm hmi_host_nvm_t;
/** @brief 启动文件 NVM 服务，NULL path 表示显式 RAM 模式。 */
hmi_host_nvm_t *hmi_host_nvm_open(hmi_core_t *, const char *, uint16_t product_namespace,
                                      uint16_t schema);
/** @brief 按 Product 的存储身份、防抖和显式参数扩展策略启动；禁用配置返回 NULL。 */
hmi_host_nvm_t *hmi_host_nvm_open_profile(hmi_core_t *, const char *, const hmi_storage_profile_t *,
                                              bool allow_parameter_extension);
/** @brief App 领取异步加载/提交结果并调度保存，不做文件 I/O。 */
void hmi_host_nvm_poll(hmi_host_nvm_t *, uint32_t now);
/** @brief 编码 App 设置副本；相同值不会重复保存。 */
bool hmi_host_nvm_changed(hmi_host_nvm_t *, uint32_t now);
/** @brief 读取当前服务状态；调用者与 poll 使用同一 App owner。 */
const hmi_nvm_service_t *hmi_host_nvm_status(const hmi_host_nvm_t *);
/** @brief 退出前有界等待目标版本 durable；超时不杀 worker、不释放仍被使用的内存。 */
bool hmi_host_nvm_close(hmi_host_nvm_t *, uint32_t timeout_ms);
#endif
