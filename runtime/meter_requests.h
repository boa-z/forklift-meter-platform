#ifndef METER_REQUESTS_H
#define METER_REQUESTS_H
#include "contracts/meter_request.h"

/** @brief INV-04/06：槽只由服务 owner 修改，worker 的值结果经原生 IPC 送回 owner。 */
typedef enum
{
    METER_REQUEST_SLOT_FREE,
    METER_REQUEST_SLOT_QUEUED,
    METER_REQUEST_SLOT_ACTIVE,
    METER_REQUEST_SLOT_TERMINAL
} meter_request_slot_state_t;

typedef struct
{
    meter_request_slot_state_t state;
    meter_request_id_t id;
    meter_request_result_t result;
} meter_request_slot_t;

typedef struct
{
    meter_request_slot_t *slots;
    size_t capacity;
    uint32_t session;
    uint64_t next_serial;
} meter_requests_t;

/**
 * @brief INV-06：启动前绑定独占静态槽；不在活动对象上重新初始化。
 * @details 全部 API 由单个服务 owner 调用，禁止 ISR/并发调用，不阻塞、不分配、不做 I/O。
 * slots 与对象必须互不重叠并覆盖整个服务期；capacity 和 session 须非零。
 * 初始化失败不修改对象；停止时须等所有已接纳结果被确认后再回收槽。
 */
bool meter_requests_init(meter_requests_t *requests, meter_request_slot_t *slots,
                         size_t capacity, uint32_t session);

/** @brief 预留一个结果槽并写入非零身份；满容量 BUSY，不写 id；耗尽身份 EXHAUSTED。 */
meter_request_admission_t meter_requests_reserve(meter_requests_t *requests, meter_request_id_t *id);

/** @brief QUEUED 转 ACTIVE 的不可取消边界；owner 在派发不可取消 I/O 前调用，失败无变化。 */
bool meter_requests_start(meter_requests_t *requests, meter_request_id_t id);

/** @brief 只取消 QUEUED 请求并保留取消终态；ACTIVE 或无效身份返回 false，不释放在途资源。 */
bool meter_requests_cancel(meter_requests_t *requests, meter_request_id_t id);

/** @brief 由 owner 记录 ACTIVE 的值结果；身份/状态不符或重复完成拒绝，不覆盖未领取终态。 */
bool meter_requests_finish(meter_requests_t *requests, const meter_request_result_t *result);

/** @brief 将终态复制到 out；未完成/无效身份不修改 out，读取不回收槽。 */
bool meter_requests_query(const meter_requests_t *requests, meter_request_id_t id,
                          meter_request_result_t *out);

/** @brief 消费者已获得结果后由 owner 确认终态并释放槽；不能确认活动请求。 */
bool meter_requests_acknowledge(meter_requests_t *requests, meter_request_id_t id);
#endif
