#include "platform/rtthread/debug/hmi_debug_console.h"
#include <assert.h>
#include <rtthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static struct rt_mutex *owner;
static char output[65536];
static size_t used;
static unsigned levels[4];
void test_native_log(int level, const char *tag, const char *format, ...)
{
    (void)format;
    assert(owner && owner->depth == 0);
    assert(tag && level >= 0 && level < 4);
    levels[level]++;
}
int rt_mutex_init(struct rt_mutex *m, const char *name, int flags)
{
    (void)flags;
    m->name = name;
    m->depth = 0;
    if (!strcmp(name, "hmi.state"))
        owner = m;
    return 0;
}
int rt_mutex_detach(struct rt_mutex *m)
{
    (void)m;
    return 0;
}
int rt_mutex_take(struct rt_mutex *m, int ticks)
{
    (void)ticks;
    assert(m->depth == 0);
    m->depth++;
    return 0;
}
int rt_mutex_release(struct rt_mutex *m)
{
    assert(m->depth == 1);
    m->depth--;
    return 0;
}
uint32_t rt_tick_get_millisecond(void)
{
    return 123;
}
int rt_kprintf(const char *format, ...)
{
    assert(!owner || owner->depth == 0);
    va_list args;
    va_start(args, format);
    int n = vsnprintf(output + used, sizeof(output) - used, format, args);
    va_end(args);
    assert(n >= 0 && (size_t)n < sizeof(output) - used);
    used += (size_t)n;
    return n;
}
extern int test_msh_hmi(int argc, char **argv);
static int query(const char *name, const char *arg)
{
    char *argv[] = {"hmi", (char *)name, (char *)arg};
    used = 0;
    output[0] = 0;
    return test_msh_hmi(arg ? 3 : 2, argv);
}
int main(void)
{
    static hmi_diagnostics_t diag;
    const hmi_build_info_t info = {.product = "test", .platform_revision = "abcdef"};
    assert(query("info", NULL) != 0);
    hmi_diagnostics_init(&diag);
    assert(hmi_debug_init(&diag, &info));
    hmi_debug_lock();
    hmi_trace_append(&diag.trace, 120, HMI_TRACE_CAN, CAN_RX_DROP, 1, 0);
    hmi_debug_unlock();
    assert(query("info", NULL) == 0 && strstr(output, "platform : abcdef"));
    assert(query("trace", "dump") == 0 && strstr(output, "CAN RX_DROP"));
    assert(diag.trace.count == 1);
    assert(query("trace", "clear now") != 0 && diag.trace.count == 1);
    assert(query("trace", "clear") == 0 && strstr(output, "cleared=1 result=OK") && diag.trace.count == 0);
    assert(query("signal", "missing") == 0 && strstr(output, "not found"));
    const char *names[] = {"diag", "runtime", "can", "domain", "touch", "trace", "storage"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        assert(query(names[i], NULL) == 0);
    /* 正常流量保持静默；WARN 限频，终态错误必须是 ERROR。 */
    hmi_debug_lock();
    hmi_trace_append(&diag.trace, 0, HMI_TRACE_DOMAIN, DOMAIN_STALE, 0, 0);
    hmi_trace_append(&diag.trace, 0, HMI_TRACE_CAN, CAN_RX_DROP, 0, 0);
    hmi_trace_append(&diag.trace, 10, HMI_TRACE_CAN, CAN_RX_DROP, 0, 0);
    hmi_trace_append(&diag.trace, 1000, HMI_TRACE_CAN, CAN_RX_DROP, 0, 0);
    hmi_trace_append(&diag.trace, 1100, HMI_TRACE_RUNTIME, RUNTIME_CONNECT, 0, 0);
    hmi_trace_append(&diag.trace, 1200, HMI_TRACE_PRODUCT, PRODUCT_FAILED, 17, 0x05040000);
    hmi_trace_append(&diag.trace, 1201, HMI_TRACE_PRODUCT, PRODUCT_READY, 0, 0);
    hmi_debug_unlock();
    hmi_debug_log_drain();
    assert(levels[0] == 0 && levels[1] == 2 && levels[2] == 2 && levels[3] == 1);
    hmi_debug_log_drain();
    assert(levels[2] == 2 && levels[3] == 1);
    assert(owner->depth == 0);
    puts("production MSH frontend: query copies, unlock-before-output, explicit trace clear PASS");
    return 0;
}

uint32_t hmi_board_now_ms(void) { return rt_tick_get_millisecond(); }
