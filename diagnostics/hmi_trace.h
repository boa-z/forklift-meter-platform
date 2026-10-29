#ifndef HMI_TRACE_H
#define HMI_TRACE_H
#include <stddef.h>
#include <stdint.h>
/** @brief 固定容量历史；静态内存约 4 KiB，无文本、无堆、无内部锁。 */
#define HMI_TRACE_CAPACITY 256u
/** @brief 模块身份与模块内事件；数值是串口工具可长期依赖的契约。 */
typedef enum
{
    HMI_TRACE_RUNTIME = 1,
    HMI_TRACE_CAN = 2,
    /* 3、4 曾是 CANopen 的 PDO/SDO 模块，编号保留不复用。 */
    HMI_TRACE_DOMAIN = 5,
    HMI_TRACE_PRODUCT = 6
} hmi_trace_module_t;
typedef enum
{
    RUNTIME_CONNECT = 1,
    RUNTIME_DISCONNECT = 2,
    /* 3、4 曾是接收队列溢出与复位，编号保留不复用。 */
    RUNTIME_INVALID_FRAME = 5
} hmi_trace_runtime_event_t;
typedef enum
{
    CAN_RX_DROP = 1,
    CAN_TX_BUSY,
    CAN_ERROR
} hmi_trace_can_event_t;
typedef enum
{
    DOMAIN_STALE = 1,
    DOMAIN_RECOVER,
    DOMAIN_SOURCE_SWITCH,
    DOMAIN_ERROR
} hmi_trace_domain_event_t;
/** @brief Product 服务结果，不携带客户状态机或业务身份。 */
typedef enum
{
    PRODUCT_READY = 1,
    PRODUCT_FAILED
} hmi_trace_product_event_t;
/** @brief 定长记录；timestamp_ms 为回绕的单调时钟，排序使用插入顺序。 */
typedef struct
{
    uint32_t timestamp_ms;
    uint16_t module, event;
    uint32_t arg0, arg1;
} hmi_trace_entry_t;
/** @brief 单写者环形存储；跨线程 append/snapshot/clear 必须由宿主串行化。 */
typedef struct
{
    hmi_trace_entry_t entries[HMI_TRACE_CAPACITY];
    uint32_t write_index, count, overwritten, sequence;
} hmi_trace_t;
/** @brief 初始化静态存储；不分配、不阻塞，不能与任何查询并发。 */
void hmi_trace_init(hmi_trace_t *trace);
/** @brief O(1) 追加并覆盖最旧条目；无格式化，调用者保证线程所有权。 */
void hmi_trace_append(hmi_trace_t *trace, uint32_t now, uint16_t module, uint16_t event, uint32_t arg0,
                        uint32_t arg1);
/** @brief 按插入顺序复制最新的至多 capacity 条；目标缓冲属于调用方。 */
size_t hmi_trace_snapshot(const hmi_trace_t *trace, hmi_trace_entry_t *out, size_t capacity);
/** @brief 清空可见历史与覆盖数，保留 sequence 供消费者跨 clear 跟踪。 */
void hmi_trace_clear(hmi_trace_t *trace);
#endif
