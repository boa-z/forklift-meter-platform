#ifndef METER_TRACE_H
#define METER_TRACE_H
#include <stddef.h>
#include <stdint.h>
/** @brief 固定容量历史；静态内存约 4 KiB，无文本、无堆、无内部锁。 */
#define METER_TRACE_CAPACITY 256u
/** @brief 模块身份与模块内事件；数值是串口工具可长期依赖的契约。 */
typedef enum
{
    METER_TRACE_RUNTIME = 1,
    METER_TRACE_CAN,
    METER_TRACE_PDO,
    METER_TRACE_SDO,
    METER_TRACE_DOMAIN,
    METER_TRACE_PRODUCT
} meter_trace_module_t;
typedef enum
{
    RUNTIME_CONNECT = 1,
    RUNTIME_DISCONNECT,
    RUNTIME_QUEUE_OVERFLOW,
    RUNTIME_RESET,
    RUNTIME_INVALID_FRAME
} meter_trace_runtime_event_t;
typedef enum
{
    CAN_RX_DROP = 1,
    CAN_TX_BUSY,
    CAN_ERROR
} meter_trace_can_event_t;
typedef enum
{
    PDO_RX = 1,
    PDO_STALE,
    PDO_RECOVER,
    PDO_DECODE_ERROR
} meter_trace_pdo_event_t;
typedef enum
{
    SDO_QUEUE = 1,
    SDO_START,
    SDO_COMPLETE,
    SDO_ABORT,
    SDO_TIMEOUT,
    SDO_RETRY,
    SDO_RESET,
    SDO_FAILED
} meter_trace_sdo_event_t;
typedef enum
{
    DOMAIN_STALE = 1,
    DOMAIN_RECOVER,
    DOMAIN_SOURCE_SWITCH,
    DOMAIN_ERROR
} meter_trace_domain_event_t;
/** @brief Product 服务结果，不携带客户状态机或业务身份。 */
typedef enum
{
    PRODUCT_READY = 1,
    PRODUCT_FAILED
} meter_trace_product_event_t;
/** @brief 定长记录；timestamp_ms 为回绕的单调时钟，排序使用插入顺序。 */
typedef struct
{
    uint32_t timestamp_ms;
    uint16_t module, event;
    uint32_t arg0, arg1;
} meter_trace_entry_t;
/** @brief 单写者环形存储；跨线程 append/snapshot/clear 必须由宿主串行化。 */
typedef struct
{
    meter_trace_entry_t entries[METER_TRACE_CAPACITY];
    uint32_t write_index, count, overwritten, sequence;
} meter_trace_t;
/** @brief 初始化静态存储；不分配、不阻塞，不能与任何查询并发。 */
void meter_trace_init(meter_trace_t *trace);
/** @brief O(1) 追加并覆盖最旧条目；无格式化，调用者保证线程所有权。 */
void meter_trace_append(meter_trace_t *trace, uint32_t now, uint16_t module, uint16_t event, uint32_t arg0,
                        uint32_t arg1);
/** @brief 按插入顺序复制最新的至多 capacity 条；目标缓冲属于调用方。 */
size_t meter_trace_snapshot(const meter_trace_t *trace, meter_trace_entry_t *out, size_t capacity);
/** @brief 清空可见历史与覆盖数，保留 sequence 供消费者跨 clear 跟踪。 */
void meter_trace_clear(meter_trace_t *trace);
#endif
