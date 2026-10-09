#define LOG_TAG "meter.runtime"
#define LOG_LVL LOG_LVL_INFO
#include "platform/common/meter_diag_commands.h"
#include "platform/rtthread/debug/meter_debug_console.h"
#include <lvgl.h>
#include <ulog.h>
/* 只在 owner 解锁后由同一线程消费，无额外线程或输出队列。 */
static meter_trace_entry_t log_copy[METER_TRACE_CAPACITY];
static uint32_t cursor, last_warning[7];
static bool warned[7];
extern size_t meter_debug_trace_since(uint32_t *cursor, meter_trace_entry_t *out, size_t capacity);
void meter_debug_log_drain(void)
{
    size_t count = meter_debug_trace_since(&cursor, log_copy, METER_TRACE_CAPACITY);
    static const char *const tags[] = {"meter.runtime", "meter.runtime", "meter.can",    "meter.pdo",
                                       "meter.sdo",     "meter.domain",  "meter.product"};
    for (size_t i = 0; i < count; ++i)
    {
        const meter_trace_entry_t *e = &log_copy[i];
        bool warning = false, info = false;
        switch (e->module)
        {
        case METER_TRACE_RUNTIME:
            warning = e->event == RUNTIME_QUEUE_OVERFLOW || e->event == RUNTIME_INVALID_FRAME;
            info = e->event == RUNTIME_CONNECT || e->event == RUNTIME_DISCONNECT;
            break;
        case METER_TRACE_CAN:
            warning = e->event == CAN_ERROR || e->event == CAN_RX_DROP;
            break;
        case METER_TRACE_PDO:
            warning = e->event == PDO_STALE || e->event == PDO_DECODE_ERROR;
            info = e->event == PDO_RECOVER;
            break;
        case METER_TRACE_SDO:
            if (e->event == SDO_FAILED)
                ulog_e("meter.sdo", "request=%lu exhausted abort=0x%08lx", (unsigned long)e->arg0,
                       (unsigned long)e->arg1);
            warning = e->event == SDO_TIMEOUT || e->event == SDO_ABORT;
            break;
        case METER_TRACE_PRODUCT:
            info = e->event == PRODUCT_READY;
            if (e->event == PRODUCT_FAILED)
                ulog_e("meter.product", "service failed code=0x%08lx", (unsigned long)e->arg1);
            break;
        default:
            break;
        }
        if (e->module >= 7)
            continue;
        /* 重复异常每模块每秒最多一条 Log；所有 Counter/Trace 保持原始观测。 */
        if (warning && (!warned[e->module] || (uint32_t)(e->timestamp_ms - last_warning[e->module]) >= 1000u))
        {
            warned[e->module] = true;
            last_warning[e->module] = e->timestamp_ms;
            ulog_w(tags[e->module], "%s arg0=%lu arg1=0x%08lx", meter_trace_event_name(e->module, e->event),
                   (unsigned long)e->arg0, (unsigned long)e->arg1);
        }
        else if (info)
            ulog_i(tags[e->module], "%s arg0=%lu", meter_trace_event_name(e->module, e->event),
                   (unsigned long)e->arg0);
    }
}
#if LV_USE_LOG
static void lvgl_log(lv_log_level_t level, const char *message)
{
    if (level == LV_LOG_LEVEL_ERROR)
        ulog_e("meter.ui", "%s", message);
    else if (level == LV_LOG_LEVEL_WARN)
        ulog_w("meter.ui", "%s", message);
    else if (level == LV_LOG_LEVEL_INFO)
        ulog_i("meter.ui", "%s", message);
    else
        ulog_d("meter.ui", "%s", message);
}
#endif
void meter_debug_lvgl_log_init(void)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lvgl_log);
#endif
}
