#ifndef METER_SLOTS_H
#define METER_SLOTS_H
#include "storage/meter_record.h"
/** @brief 后端错误；不把 NACK、写保护、超时或同步失败压成保存成功。 */
typedef enum
{
    METER_IO_OK,
    METER_IO_ERROR,
    METER_IO_TIMEOUT,
    METER_IO_RANGE,
    METER_IO_READ_ONLY,
    METER_IO_VERIFY,
    METER_IO_UNSUPPORTED
} meter_io_result_t;
/** @brief 同步后端仅由 NVM worker 调用；每次传输必须完整成功。 */
typedef struct
{
    void *context;
    meter_io_result_t (*read)(void *, size_t, uint8_t *, size_t);
    meter_io_result_t (*write)(void *, size_t, const uint8_t *, size_t);
    meter_io_result_t (*sync)(void *);
    size_t capacity, page_size;
    const char *name;
    bool power_safe;
} meter_nvm_io_t;
/** @brief 扫描与提交结果；UNKNOWN/IO_ERROR 不得被当作空白介质初始化。 */
typedef enum
{
    METER_SLOTS_OK,
    METER_SLOTS_EMPTY,
    METER_SLOTS_INVALID,
    METER_SLOTS_CAPACITY,
    METER_SLOTS_WRITE_INTERRUPTED,
    METER_SLOTS_NO_VALID_SLOT,
    METER_SLOTS_INCOMPATIBLE,
    METER_SLOTS_IO_ERROR,
    METER_SLOTS_CONFLICT
} meter_slots_result_t;
typedef struct
{
    uint8_t *media;
    size_t media_size;
    uint8_t *scratch;
    size_t scratch_size, slot_size, active_slot;
    meter_record_view_t active;
    bool has_active;
    const meter_nvm_io_t *io;
    size_t seal_page;
    bool writable, degraded;
    meter_io_result_t last_error;
} meter_slots_t;
/** @brief 绑定 Host 撕裂介质模型；调用方缓冲覆盖对象生命周期。 */
bool meter_slots_init(meter_slots_t *, uint8_t *, size_t, uint8_t *, size_t);
/** @brief 绑定实际后端及双槽读缓存；槽与 seal 页必须物理隔离。 */
bool meter_slots_bind(meter_slots_t *, const meter_nvm_io_t *);
/** @brief 只读扫描，校验所有候选与相同代次冲突；未知 schema 禁止自动覆盖。 */
meter_slots_result_t meter_slots_scan(meter_slots_t *, uint16_t product_namespace, uint16_t schema);
/** @brief 失效 seal、同步读回、写 body、同步读回、写 seal、同步读回后才更新活动槽。 */
meter_slots_result_t meter_slots_commit(meter_slots_t *, const meter_record_view_t *,
                                        size_t fail_after_bytes);
/** @brief 获取活动记录视图；指针有效期截至下一次扫描或提交。 */
bool meter_slots_active(const meter_slots_t *, meter_record_view_t *);
/** @brief 显式工厂初始化仅限无有效槽；调用前必须备份并取得布局授权。 */
meter_slots_result_t meter_slots_initialize_empty(meter_slots_t *);
#endif
