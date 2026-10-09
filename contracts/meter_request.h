#ifndef METER_REQUEST_H
#define METER_REQUEST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief INV-06：session 非零、serial 非零且同一 session 不复用；不承载业务数据。 */
typedef struct
{
    uint32_t session;
    uint64_t serial;
} meter_request_id_t;

/** @brief 接纳只预留完成空间，不表示已应用、已发送或已持久化。 */
typedef enum
{
    METER_REQUEST_QUEUED,
    METER_REQUEST_BUSY,
    METER_REQUEST_INVALID,
    METER_REQUEST_EXHAUSTED
} meter_request_admission_t;

/** @brief 服务按语义选择终态；OUTCOME_UNKNOWN 不代表回滚成功。 */
typedef enum
{
    METER_RESULT_APPLIED,
    METER_RESULT_REJECTED,
    METER_RESULT_DURABLE,
    METER_RESULT_DRIVER_COMPLETED,
    METER_RESULT_FAILED,
    METER_RESULT_OUTCOME_UNKNOWN,
    METER_RESULT_CANCELLED_BEFORE_IO,
    METER_RESULT_EXPIRED_BEFORE_SEND,
    METER_RESULT_SUPERSEDED
} meter_result_code_t;

/** @brief 有类型的结果元数据；revision 的业务含义由对应服务声明，不借用 payload 指针。 */
typedef struct
{
    meter_request_id_t id;
    meter_result_code_t code;
    uint64_t revision;
    int32_t detail;
} meter_request_result_t;

/** @brief 比较本地身份的纯函数，任意 owner 可调用；不能据此识别全部线上旧响应。 */
static inline bool meter_request_id_equal(meter_request_id_t first, meter_request_id_t second)
{
    return (first.session == second.session) && (first.serial == second.serial);
}
#endif
