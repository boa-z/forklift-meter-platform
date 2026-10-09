#ifndef METER_DIAGNOSTICS_H
#define METER_DIAGNOSTICS_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "diagnostics/meter_trace.h"
/** @brief 保留原 Runtime counters 并追加 resets，避免两套计数真源。 */
typedef struct
{
    uint32_t accepted, overflow, malformed, unrouted, decode_failed, dispatched, resets;
} meter_runtime_diagnostics_t;
/** @brief Runtime 对外状态，无私有指针。 */
typedef struct
{
    bool available, connected;
    uint32_t generation;
    size_t depth, capacity, adapters;
    meter_runtime_diagnostics_t counters;
} meter_diag_runtime_t;
/** @brief 总线统计；available 表示已接入观测，board_available 表示硬件状态已知。 */
typedef struct
{
    bool available, board_available, open, rx_seen, tx_seen;
    uint32_t bitrate, rx, tx, rx_drop, tx_busy, rx_error, tx_error, last_rx_ms, last_tx_ms;
} meter_diag_can_t;
/** @brief 静态 PDO 绑定聚合统计；默认不为正常每帧生成 Trace。 */
typedef struct
{
    bool available, seen, stale_state;
    uint32_t bindings, timeout_ms, last_rx_ms, rx, tx, decode_error, stale, recover;
} meter_diag_pdo_t;
/** @brief SDO 当前通道与累计统计；operation 为 0=READ、1=WRITE，state 对应调度器生命周期。 */
typedef struct
{
    bool available;
    meter_bus_role_t bus;
    uint8_t node, subindex, operation, state;
    uint16_t index, attempt;
    uint32_t active_request, last_abort, queued, started, completed, aborted, timeout, retry, tx_busy,
        queue_full, reset;
    size_t depth;
} meter_diag_sdo_t;
/** @brief Domain 统计不改变 Domain revision；结构规模来自产品目录。 */
typedef struct
{
    bool available, connected;
    size_t signals, parameters, faults;
    uint32_t generation, revision;
    uint32_t updates, stale_transition, valid_transition, source_switch, error_updates;
} meter_diag_domain_t;
/** @brief UI 低成本统计，flush_available 区分尚未接入的后端。 */
typedef struct
{
    bool available, flush_available;
    uint32_t present_count, flush_count;
} meter_diag_ui_t;
/** @brief 复用触摸驱动公开诊断值，禁止查询时触发新的触摸采样。 */
typedef struct
{
    bool available;
    int32_t range_x, range_y, x, y, state;
    uint32_t irq, reads, events, delivered, recovered, empty_reads, invalid_reads;
} meter_diag_touch_t;
/** @brief NVM 后端可用性与计数，RAM 设置不假装已落盘。 */
typedef struct
{
    bool available;
    uint32_t reads, writes, errors;
    const char *backend;
    uint32_t state, last_error, last_result, depth;
    bool dirty, degraded, imperial;
    uint32_t language, brightness, configured_can_rate;
    uint64_t ram_revision, inflight_revision, durable_revision;
} meter_diag_storage_t;
/** @brief 一次统一查询结果；所有 unavailable 模块保持零值。 */
typedef struct
{
    uint32_t uptime_ms;
    meter_diag_runtime_t runtime;
    meter_diag_can_t can[METER_BUS_COUNT];
    meter_diag_pdo_t pdo;
    meter_diag_sdo_t sdo;
    meter_diag_domain_t domain;
    meter_diag_ui_t ui;
    meter_diag_touch_t touch;
    meter_diag_storage_t storage;
    uint32_t trace_capacity, trace_count, trace_write_index, trace_overwritten;
} meter_diag_snapshot_t;
/** @brief 固件身份字符串由构建系统提供，生命周期覆盖程序运行期。 */
typedef struct
{
    const char *product, *platform_revision, *sdk_revision, *lvgl_version, *lvgl_aic_revision, *board,
        *build_date, *build_time, *firmware_version, *display_version;
} meter_build_info_t;
/** @brief 调用者持有的静态诊断实例；字段仅允许 owner 更新，跨线程由宿主锁保护。 */
typedef struct meter_diagnostics
{
    meter_diag_snapshot_t data;
    meter_trace_t trace;
    const meter_snapshot_t *domain;
} meter_diagnostics_t;
/** @brief 查询信号的值拷贝；key/unit 指向生命周期覆盖查询的只读产品目录。 */
typedef struct
{
    uint16_t id;
    const char *key, *unit;
    meter_value_t value;
    uint32_t age_ms, stale_ms;
} meter_diag_signal_t;
/** @brief 初始化实例，不分配、不阻塞；不得覆盖仍被模块绑定的实例。 */
void meter_diagnostics_init(meter_diagnostics_t *diag);
/** @brief 更新单调时间；通过无符号差计算 age，要求两次观察间隔小于 2^32 ms。 */
void meter_diagnostics_time(meter_diagnostics_t *diag, uint32_t now);
/** @brief 拷贝一致的状态；调用者必须与全部写者串行化，函数不持内部 OS 锁。 */
bool meter_diagnostics_snapshot(const meter_diagnostics_t *diag, uint32_t now, meter_diag_snapshot_t *out);
/** @brief 按 canonical key 查询 Domain 公共快照，未找到返回 false；无控制副作用。 */
bool meter_diagnostics_signal(const meter_diagnostics_t *diag, const char *key, uint32_t now,
                              meter_diag_signal_t *out);
/** @brief 饱和递增，长期运行不回绕；只由所属 owner 调用。 */
static inline void meter_diag_increment(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
        ++*counter;
}
/** @brief 可选观测点；未绑定诊断实例时不做任何操作。 */
#define METER_DIAG_INC(d, group, field)                                                                      \
    do                                                                                                       \
    {                                                                                                        \
        if (d)                                                                                               \
            meter_diag_increment(&(d)->data.group.field);                                                    \
    } while (0)
/** @brief 结构化事件入口，无字符串格式化或输出。 */
#define METER_DIAG_TRACE(d, module, event, now, a, b)                                                        \
    do                                                                                                       \
    {                                                                                                        \
        if (d)                                                                                               \
            meter_trace_append(&(d)->trace, now, module, event, a, b);                                       \
    } while (0)
/** @brief CAN 观测类型；TX_BUSY 为端口暂未接受，不等同于硬件错误。 */
typedef enum
{
    METER_CAN_RX,
    METER_CAN_TX,
    METER_CAN_RX_DROP,
    METER_CAN_TX_BUSY,
    METER_CAN_RX_ERROR,
    METER_CAN_TX_ERROR
} meter_diag_can_event_t;
/** @brief 在驱动/队列边界记录总线统计，正常 RX/TX 只累计 Counter。 */
void meter_diagnostics_can(meter_diagnostics_t *diag, meter_bus_role_t bus, meter_diag_can_event_t event,
                           uint32_t now);
#endif
