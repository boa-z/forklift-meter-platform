#define LOG_TAG "meter.wdog"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_watchdog_port.h"
#include "platform/rtthread/meter_watchdog_budget.h"
#include "platform/rtthread/meter_execution_port.h"
#include "runtime/meter_watchdog.h"
#include <rtdevice.h>
#include <rtthread.h>
#include <ulog.h>

/* 启用开关来自 Kconfig；关掉时端口不编译，只留一个记录用的空实现。 */
#ifdef AIC_FORKLIFT_WATCHDOG

/* 监督线程是该文件唯一的写者，报告与诊断只读拷贝，因此状态不需要锁。 */
static rt_device_t wdt_device;
static struct rt_thread watchdog_thread;
static rt_ubase_t watchdog_stack[METER_WATCHDOG_THREAD_STACK / sizeof(rt_ubase_t)];
static meter_watchdog_state_t state;
static meter_watchdog_policy_t policy;
static bool device_present, armed;
static uint32_t report_feeds, report_withheld, report_stalls, report_transitions, report_stalled;

static const char *const channel_names[METER_WATCHDOG_CHANNELS] = {"protocol", "app", "ui"};

static bool arm_watchdog(void)
{
    uint16_t timeout = (uint16_t)METER_WATCHDOG_TIMEOUT_S;
    if (rt_device_control(wdt_device, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout) != RT_EOK)
    {
        LOG_E("cannot set WDT timeout to %us", (unsigned)METER_WATCHDOG_TIMEOUT_S);
        return false;
    }
    if (rt_device_control(wdt_device, RT_DEVICE_CTRL_WDT_START, RT_NULL) != RT_EOK)
    {
        LOG_E("cannot start WDT");
        return false;
    }
    LOG_I("WDT armed: timeout=%us poll=%ums stall protocol/app=%ums ui=%ums",
          (unsigned)METER_WATCHDOG_TIMEOUT_S, (unsigned)METER_WATCHDOG_POLL_MS,
          (unsigned)METER_WATCHDOG_PROTOCOL_STALL_MS, (unsigned)METER_WATCHDOG_UI_STALL_MS);
    return true;
}

static void watchdog_entry(void *parameter)
{
    (void)parameter;
    uint32_t last_report = 0u;
    for (;;)
    {
        meter_execution_liveness_t live;
        /* runtime 未启动或正在停止时不进入监督态：启动失败与技师发起的停机都不得复位。 */
        if (meter_execution_liveness(&live) && live.started && !live.stopping)
        {
            if (!armed)
                armed = arm_watchdog();
            if (armed)
            {
                meter_watchdog_input_t input = {0};
                input.counter[METER_WATCHDOG_PROTOCOL] = live.protocol_runs;
                input.counter[METER_WATCHDOG_APP] = live.app_runs;
                input.counter[METER_WATCHDOG_UI] = live.ui_ticks;
                input.expected = (1u << METER_WATCHDOG_PROTOCOL) | (1u << METER_WATCHDOG_APP) |
                                 (1u << METER_WATCHDOG_UI);
                const uint32_t now = (uint32_t)rt_tick_get_millisecond();
                if (meter_watchdog_step(&state, &policy, &input, now))
                    (void)rt_device_control(wdt_device, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL);
                else if (now - last_report > 1000u)
                {
                    /* 状态变化后仍停滞，最多每秒上报一次，避免监督线程自己刷屏。 */
                    last_report = now;
                    const bool known = state.stalled < METER_WATCHDOG_CHANNELS;
                    LOG_E("feed withheld: %s stalled over %ums; hardware reset follows",
                          known ? channel_names[state.stalled] : "unknown",
                          known ? (unsigned)policy.stall_ms[state.stalled] : 0u);
                }
            }
        }
        report_feeds = state.feeds;
        report_withheld = state.withheld;
        report_stalls = state.stalls;
        report_transitions = state.transitions;
        report_stalled = (uint32_t)state.stalled;
        rt_thread_mdelay(METER_WATCHDOG_POLL_MS);
    }
}

int meter_watchdog_port_init(void)
{
    wdt_device = rt_device_find("wdt");
    if (!wdt_device)
    {
        /* 没有看门狗设备的板子保持关闭，不影响其余功能。 */
        LOG_W("no wdt device; watchdog supervision stays off");
        return RT_EOK;
    }
    if (rt_device_init(wdt_device) != RT_EOK)
    {
        LOG_E("cannot initialize wdt device; supervision stays off");
        wdt_device = RT_NULL;
        return RT_EOK;
    }
    device_present = true;
    policy.enabled = true;
    policy.stall_ms[METER_WATCHDOG_PROTOCOL] = METER_WATCHDOG_PROTOCOL_STALL_MS;
    policy.stall_ms[METER_WATCHDOG_APP] = METER_WATCHDOG_APP_STALL_MS;
    policy.stall_ms[METER_WATCHDOG_UI] = METER_WATCHDOG_UI_STALL_MS;
    meter_watchdog_init(&state);
    if (rt_thread_init(&watchdog_thread, "meter_wdog", watchdog_entry, RT_NULL, watchdog_stack,
                       sizeof(watchdog_stack), METER_WATCHDOG_THREAD_PRIO, 10) != RT_EOK ||
        rt_thread_startup(&watchdog_thread) != RT_EOK)
        LOG_E("cannot start watchdog thread; supervision stays off");
    return RT_EOK;
}

bool meter_watchdog_report(meter_watchdog_report_t *out)
{
    if (!out || !device_present)
        return false;
    *out = (meter_watchdog_report_t){.device_present = true,
                                     .armed = armed,
                                     .supervising = state.supervising,
                                     .feeds = report_feeds,
                                     .withheld = report_withheld,
                                     .stalls = report_stalls,
                                     .transitions = report_transitions,
                                     .stalled_channel = report_stalled,
                                     .timeout_s = (uint16_t)METER_WATCHDOG_TIMEOUT_S};
    return true;
}

static int meter_watchdog_msh(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    meter_watchdog_report_t report;
    if (!meter_watchdog_report(&report))
    {
        rt_kprintf("watchdog unavailable (no wdt device or disabled)\n");
        return -RT_ERROR;
    }
    rt_kprintf("watchdog armed=%u supervising=%u timeout_s=%u\n", report.armed ? 1u : 0u,
               report.supervising ? 1u : 0u, (unsigned)report.timeout_s);
    rt_kprintf("watchdog feeds=%u withheld=%u stalls=%u transitions=%u stalled=%u\n", report.feeds,
               report.withheld, report.stalls, report.transitions, report.stalled_channel);
    for (unsigned i = 0u; i < METER_WATCHDOG_CHANNELS; ++i)
        rt_kprintf("watchdog channel=%s stall_ms=%u\n", channel_names[i], (unsigned)policy.stall_ms[i]);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(meter_watchdog_msh, meter_watchdog, Show watchdog supervision state);

#else

int meter_watchdog_port_init(void)
{
    LOG_W("watchdog supervision is disabled by AIC_FORKLIFT_WATCHDOG");
    return RT_EOK;
}

bool meter_watchdog_report(meter_watchdog_report_t *out)
{
    (void)out;
    return false;
}

#endif
INIT_APP_EXPORT(meter_watchdog_port_init);
