#ifndef METER_NVM_H
#define METER_NVM_H
#include "storage/meter_slots.h"
/** @brief App 发布的持久状态；介质加载、排队与实际提交彼此独立。 */
typedef enum
{
    METER_NVM_LOADING,
    METER_NVM_EMPTY,
    METER_NVM_READY,
    METER_NVM_DIRTY,
    METER_NVM_IN_PROGRESS,
    METER_NVM_DURABLE,
    METER_NVM_FAILED,
    METER_NVM_UNCERTAIN,
    METER_NVM_CORRUPT,
    METER_NVM_INCOMPATIBLE,
    METER_NVM_IO_ERROR,
    METER_NVM_CANCELLED
} meter_nvm_state_t;
/** @brief 在途任务只引用专用不可变缓冲；完成前不得复用该缓冲。 */
typedef struct
{
    uint32_t generation;
    uint64_t revision;
    meter_record_view_t record;
} meter_nvm_job_t;
/** @brief App 独占状态，worker 仅接收 job 并返回结果，不读取 Core 或服务对象。 */
typedef struct
{
    uint8_t *pending, *inflight;
    size_t capacity, pending_size;
    uint16_t type, product_namespace, schema;
    uint32_t generation, debounce_ms, max_delay_ms, first_dirty_ms, last_change_ms;
    uint64_t ram_revision, inflight_revision, durable_revision;
    meter_nvm_state_t state;
    meter_slots_result_t last_result;
    bool dirty, busy, writable, immediate, observed;
} meter_nvm_service_t;
/** @brief 初始化 App 状态与两个产品容量缓冲；不分配、不访问介质。 */
bool meter_nvm_init(meter_nvm_service_t *, uint8_t *, uint8_t *, size_t, uint16_t type,
                    uint16_t product_namespace, uint16_t schema, uint32_t debounce_ms, uint32_t max_delay_ms);
/** @brief 只接纳同 generation 且 RAM 未修改的启动结果；payload 已由 App 完整验证。 */
bool meter_nvm_loaded(meter_nvm_service_t *, uint32_t generation, meter_slots_result_t,
                      const meter_record_view_t *);
/** @brief 接纳 App 完整偏好副本；内容不变不增版，连续变化也受最大延迟限制。 */
bool meter_nvm_observe(meter_nvm_service_t *, const uint8_t *, size_t, uint32_t now);
/** @brief 显式保存只提前提交时机；不修改 RAM 版本、不同步等待 I/O。 */
void meter_nvm_request_save(meter_nvm_service_t *);
/** @brief 返回到期任务，固定在途副本直到匹配完成；调用者负责可靠投递。 */
bool meter_nvm_take(meter_nvm_service_t *, uint32_t now, meter_nvm_job_t *);
/** @brief 仅匹配 generation/revision 的完成能推进 durable；失败不清 dirty。 */
bool meter_nvm_complete(meter_nvm_service_t *, uint32_t generation, uint64_t revision, meter_slots_result_t);
/** @brief 显式重读介质后协调不确定结果；保留新 RAM 值，不当作启动加载覆盖。 */
bool meter_nvm_reconcile(meter_nvm_service_t *, meter_slots_result_t, const meter_record_view_t *);
/** @brief 仅取消尚未交 worker 的保存；已进入 I/O 的请求不可伪造取消。 */
bool meter_nvm_cancel(meter_nvm_service_t *);
/** @brief 检查具体目标版本是否已由完整替代记录覆盖；新版本 dirty 不抹掉旧确认。 */
bool meter_nvm_barrier(const meter_nvm_service_t *, uint64_t target);
/** @brief 用于只读诊断的稳定状态名称。 */
const char *meter_nvm_state_name(meter_nvm_state_t);
#endif
