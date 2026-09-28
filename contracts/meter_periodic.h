#ifndef METER_PERIODIC_H
#define METER_PERIODIC_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "contracts/meter_execution.h"
/** @brief Product 定义单位的整数语义；采样时间不因再次发布而变化。 */
typedef struct { int32_t value; uint32_t sample_ms; bool valid; } meter_tx_value_t;
/** @brief 调用方拥有数组；跨 owner 必须在短锁内深复制，不借可变指针。 */
typedef struct
{
    uint32_t generation, published_ms;
    uint64_t revision;
    size_t count;
    meter_tx_value_t *values;
} meter_tx_publication_t;
/** @brief Encoder 只读语义视图，禁止通过类型访问可变样本。 */
typedef struct
{
    uint32_t generation, published_ms;
    uint64_t revision;
    size_t count;
    const meter_tx_value_t *values;
} meter_tx_snapshot_t;
/** @brief App 从域复制业务值；不得编码 CAN；失败时不发布部分结果。 */
typedef bool (*meter_tx_sample_fn_t)(const meter_snapshot_t *, uint32_t, meter_tx_value_t *, size_t);
/** @brief 新鲜度由依赖样本决定；ENCODE 将 fresh=false 交给 Product 决定标志或替代值。 */
typedef enum { METER_TX_HOLD, METER_TX_ENCODE_INVALID, METER_TX_SUPPRESS } meter_tx_freshness_t;
/** @brief 持续状态默认跳过忙槽；REPLACE 只替代尚未送入驱动的槽，不撤销正在发送的帧。 */
typedef enum { METER_TX_SKIP_BUSY, METER_TX_REPLACE_PENDING } meter_tx_backlog_t;
/** @brief 只支持入队或本机驱动完成推进；不把驱动结果当作远端业务确认。 */
typedef enum { METER_TX_COMMIT_ADMISSION, METER_TX_COMMIT_DRIVER } meter_tx_commit_t;
/** @brief Protocol 同步有界纯转换；wire 为 Product 定义的 32 位状态，不访问 Core/IPC/I/O。 */
typedef bool (*meter_periodic_encoder_t)(const meter_tx_snapshot_t *, bool fresh,
    uint32_t wire, meter_can_frame_t *, uint32_t *next_wire);
/** @brief 不可变 Product 周期定义；frame 提供固定报文或动态报文的总线/身份模板。 */
typedef struct
{
    meter_can_frame_t frame;
    uint32_t period_ms;
    bool critical;
    meter_periodic_encoder_t encode;
    size_t first_value, value_count;
    uint32_t max_age_ms, expiry_ms, initial_wire;
    meter_tx_freshness_t freshness;
    meter_tx_backlog_t backlog;
    meter_tx_commit_t commit;
} meter_periodic_frame_t;
/** @brief 按值传递的发送身份；expiry 仅限制进入驱动，不能取消硬件已开始的发送。 */
typedef struct
{
    size_t entry;
    uint32_t generation, deadline_ms, expiry_ms, queued_ms, acquired_ms, published_ms;
    uint64_t ticket, revision;
    meter_can_frame_t frame;
} meter_periodic_message_t;
/** @brief TX owner 只写身份及驱动结果，Protocol 消费后才提交 wire state。 */
typedef struct
{
    uint32_t generation, started_ms, completed_ms;
    uint64_t ticket;
    bool success;
} meter_periodic_result_t;
#endif
