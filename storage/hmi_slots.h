#ifndef HMI_SLOTS_H
#define HMI_SLOTS_H
#include "storage/hmi_record.h"
/** @brief 后端错误；不把 NACK、写保护、超时或同步失败压成保存成功。 */
typedef enum
{
    HMI_IO_OK,
    HMI_IO_ERROR,
    HMI_IO_TIMEOUT,
    HMI_IO_RANGE,
    HMI_IO_READ_ONLY,
    HMI_IO_VERIFY,
    HMI_IO_UNSUPPORTED
} hmi_io_result_t;
/** @brief 同步后端仅由 NVM worker 调用；每次传输必须完整成功。 */
typedef struct
{
    void *context;
    hmi_io_result_t (*read)(void *, size_t, uint8_t *, size_t);
    hmi_io_result_t (*write)(void *, size_t, const uint8_t *, size_t);
    hmi_io_result_t (*sync)(void *);
    size_t capacity, page_size;
    const char *name;
    bool power_safe;
} hmi_nvm_io_t;
/** @brief 扫描与提交结果；UNKNOWN/IO_ERROR 不得被当作空白介质初始化。 */
typedef enum
{
    HMI_SLOTS_OK,
    HMI_SLOTS_EMPTY,
    HMI_SLOTS_INVALID,
    HMI_SLOTS_CAPACITY,
    HMI_SLOTS_WRITE_INTERRUPTED,
    HMI_SLOTS_NO_VALID_SLOT,
    HMI_SLOTS_INCOMPATIBLE,
    HMI_SLOTS_IO_ERROR,
    HMI_SLOTS_CONFLICT
} hmi_slots_result_t;
typedef struct
{
    uint8_t *media;
    size_t media_size;
    uint8_t *scratch;
    size_t scratch_size, slot_size, active_slot;
    hmi_record_view_t active;
    bool has_active;
    const hmi_nvm_io_t *io;
    size_t seal_page;
    bool writable, degraded;
    /* 置位后扫描结果不再决定能否提交：无有效槽时从 A 槽重新开始写，有效槽仍不被覆盖；
       另一槽是不兼容记录时仍选用有效槽。 */
    bool force_writable;
    /* 最近一次扫描见到的最大有效序号（含同代次冲突）；无有效槽时为 0。 */
    uint64_t max_sequence;
    hmi_io_result_t last_error;
} hmi_slots_t;
/** @brief 绑定 Host 撕裂介质模型；调用方缓冲覆盖对象生命周期。 */
bool hmi_slots_init(hmi_slots_t *, uint8_t *, size_t, uint8_t *, size_t);
/** @brief 绑定实际后端及双槽读缓存；槽与 seal 页必须物理隔离。 */
bool hmi_slots_bind(hmi_slots_t *, const hmi_nvm_io_t *);
/** @brief 只读扫描，校验所有候选与相同代次冲突；未知 schema 默认禁止自动覆盖（force_writable 除外）。 */
hmi_slots_result_t hmi_slots_scan(hmi_slots_t *, uint16_t product_namespace, uint16_t schema);
/** @brief 失效 seal、同步读回、写 body、同步读回、写 seal、同步读回后才更新活动槽。 */
hmi_slots_result_t hmi_slots_commit(hmi_slots_t *, const hmi_record_view_t *,
                                        size_t fail_after_bytes);
/** @brief 获取活动记录视图；指针有效期截至下一次扫描或提交。 */
bool hmi_slots_active(const hmi_slots_t *, hmi_record_view_t *);
/** @brief 显式工厂初始化仅限无有效槽；调用前必须备份并取得布局授权。 */
hmi_slots_result_t hmi_slots_initialize_empty(hmi_slots_t *);
#endif
