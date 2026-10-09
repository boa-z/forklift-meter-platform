#ifndef METER_BATCH_H
#define METER_BATCH_H
#include "contracts/meter_domain.h"

/** @brief RT-APP-01：一帧的完整语义视图；接收方必须复制，不能跨线程保存此指针。 */
typedef struct
{
    uint32_t generation;
    uint64_t sequence;
    uint32_t received_ms;
    uint8_t bus;
    meter_source_id_t source;
    const meter_update_t *updates;
    size_t count;
} meter_update_batch_t;

/** @brief 入队与业务来源仲裁不同；QUEUED 不表示 App 已接受数值/来源。 */
typedef enum
{
    METER_BATCH_QUEUED,
    METER_BATCH_IPC_FULL,
    METER_BATCH_INVALID,
    METER_BATCH_TOO_LARGE,
    METER_BATCH_DECODE_FAILED
} meter_batch_result_t;

/** @brief Protocol 同步调用；成功前完整复制到自有有界 IPC，不调用 Core，不保存借用指针。 */
typedef meter_batch_result_t (*meter_batch_submit_fn_t)(void *context, const meter_update_batch_t *batch);
#endif
