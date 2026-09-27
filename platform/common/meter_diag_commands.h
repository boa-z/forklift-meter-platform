#ifndef METER_DIAG_COMMANDS_H
#define METER_DIAG_COMMANDS_H
#include "diagnostics/meter_diagnostics.h"
/** @brief MSH 已分词后的查询类型；不实现 UART 或交互 Shell。 */
typedef enum
{
    METER_QUERY_INFO,
    METER_QUERY_DIAG,
    METER_QUERY_RUNTIME,
    METER_QUERY_CAN,
    METER_QUERY_PDO,
    METER_QUERY_SDO,
    METER_QUERY_DOMAIN,
    METER_QUERY_SIGNAL,
    METER_QUERY_TOUCH,
    METER_QUERY_TRACE,
    METER_QUERY_TRACE_DUMP,
    METER_QUERY_TRACE_CLEAR
} meter_diag_query_kind_t;
/** @brief key 借用 argv 生命周期；bus=-1 表示全部总线。 */
typedef struct
{
    meter_diag_query_kind_t kind;
    int bus;
    const char *key;
} meter_diag_query_t;
/** @brief 一次查询使用的副本；render 期间调用者持有全部缓冲，不能指向正在变化的私有对象。 */
typedef struct
{
    const meter_diag_snapshot_t *snapshot;
    const meter_build_info_t *build;
    const meter_diag_signal_t *signal;
    const meter_trace_entry_t *entries;
    size_t entry_count;
    uint32_t cleared;
} meter_diag_view_t;
/** @brief 同步输出一行；文本只在回调期间有效，仅在线程查询上下文使用。 */
typedef void (*meter_diag_line_fn)(void *context, const char *line);
/** @brief 校验原生 MSH argv；不修改状态，未知或多余参数返回 false。 */
bool meter_diag_query(int argc, const char *const *argv, meter_diag_query_t *out);
/** @brief 格式化已复制的查询结果；无车辆控制副作用，输出阻塞行为由前端决定。 */
void meter_diag_render(const meter_diag_query_t *query, const meter_diag_view_t *view,
                       meter_diag_line_fn emit, void *context);
/** @brief 将稳定的模块、事件编号映射为显示名称，未知编号保留数字可供排障。 */
const char *meter_trace_module_name(uint16_t module);
/** @brief 返回静态事件名；无分配，调用者无需释放。 */
const char *meter_trace_event_name(uint16_t module, uint16_t event);
#endif
