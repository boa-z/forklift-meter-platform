#include "platform/rtthread/debug/meter_debug_console.h"
#include "platform/common/meter_diag_commands.h"
#include <finsh.h>
#include <rtthread.h>
static struct rt_mutex owner_mutex, console_mutex;
static meter_diagnostics_t *diagnostics;
static const meter_build_info_t *identity;
/* 查询副本不占 MSH 小栈；console mutex 保护并发的命令调用。 */
static meter_trace_entry_t entries[METER_TRACE_CAPACITY];
static meter_diag_snapshot_t snapshot;
static meter_diag_signal_t signal;
bool meter_debug_init(meter_diagnostics_t *diag, const meter_build_info_t *build)
{
    if (!diag || !build || diagnostics)
        return false;
    if (rt_mutex_init(&owner_mutex, "meter.state", RT_IPC_FLAG_PRIO) != RT_EOK)
        return false;
    if (rt_mutex_init(&console_mutex, "meter.query", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        rt_mutex_detach(&owner_mutex);
        return false;
    }
    identity = build;
    diagnostics = diag;
    return true;
}
void meter_debug_lock(void)
{
    rt_mutex_take(&owner_mutex, RT_WAITING_FOREVER);
}
void meter_debug_unlock(void)
{
    rt_mutex_release(&owner_mutex);
}
static void output(void *ctx, const char *line)
{
    (void)ctx;
    /* 这是 MSH 展示端；业务路径和锁内不调用 Console。 */
    rt_kprintf("%s\n", line);
}
static int meter(int argc, char **argv)
{
    meter_diag_query_t query;
    if (!meter_diag_query(argc, (const char *const *)argv, &query))
    {
        output(NULL, "usage: meter info|diag|runtime|can [0|1]|pdo|sdo|domain|signal "
                     "<key>|touch|storage|trace [dump|clear]");
        return -RT_EINVAL;
    }
    if (!diagnostics)
    {
        output(NULL, "meter diagnostics not initialized");
        return -RT_ERROR;
    }
    rt_mutex_take(&console_mutex, RT_WAITING_FOREVER);
    meter_diag_view_t view = {.snapshot = &snapshot, .build = identity};
    meter_debug_lock();
    uint32_t now = (uint32_t)rt_tick_get_millisecond();
    meter_diagnostics_snapshot(diagnostics, now, &snapshot);
    if (query.kind == METER_QUERY_SIGNAL && meter_diagnostics_signal(diagnostics, query.key, now, &signal))
        view.signal = &signal;
    if (query.kind == METER_QUERY_TRACE_DUMP)
    {
        view.entries = entries;
        view.entry_count = meter_trace_snapshot(&diagnostics->trace, entries, METER_TRACE_CAPACITY);
    }
    if (query.kind == METER_QUERY_TRACE_CLEAR)
    {
        view.cleared = diagnostics->trace.count;
        meter_trace_clear(&diagnostics->trace);
    }
    meter_debug_unlock();
    meter_diag_render(&query, &view, output, NULL);
    rt_mutex_release(&console_mutex);
    return RT_EOK;
}
MSH_CMD_EXPORT(meter, Meter diagnostics queries and trace history);
/* 后端只取得副本，不把诊断内部指针暴露给 ULog。 */
size_t meter_debug_trace_since(uint32_t *cursor, meter_trace_entry_t *out, size_t capacity)
{
    if (!diagnostics || !cursor || !out)
        return 0;
    meter_debug_lock();
    uint32_t delta = diagnostics->trace.sequence - *cursor;
    if (delta > diagnostics->trace.count)
        delta = diagnostics->trace.count;
    if (delta > capacity)
        delta = (uint32_t)capacity;
    size_t count = meter_trace_snapshot(&diagnostics->trace, out, delta);
    *cursor = diagnostics->trace.sequence;
    meter_debug_unlock();
    return count;
}
