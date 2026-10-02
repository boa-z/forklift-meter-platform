#define LOG_TAG "meter.rtc"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_rtc_port.h"
#include "contracts/meter_wall_clock.h"
#include <rtthread.h>
#include <drivers/rtc.h>
#include <time.h>
#include <ulog.h>

/* 读缓存提到文件作用域：对时成功后要立刻把新值写回缓存，否则限流窗口内界面还在显示旧时间，
   用户会以为没写进去。缓存无锁：墙上时钟只由 UI 线程经 present() 读取和设置。 */
static meter_wall_time_t cached_now;
static bool cached_valid;
static rt_tick_t last_poll;

static bool rtc_sample_utc(meter_wall_time_t *out)
{
    const time_t raw = time(RT_NULL);
    /* 越界读数按未设置处理，界面继续显示 --:-- 而不是 1970 或 2020。 */
    if (raw == (time_t)-1 || (long long)raw <= METER_RTC_FLOOR_UTC)
        return false;
    struct tm utc;
    /* gmtime_r 只做纯日历换算，不读时区；本端口固定输出 UTC，区域平移由使用方负责。 */
    if (!gmtime_r(&raw, &utc))
        return false;
    out->year = (uint16_t)(utc.tm_year + 1900);
    out->month = (uint8_t)(utc.tm_mon + 1);
    out->day = (uint8_t)(utc.tm_mday);
    out->hour = (uint8_t)(utc.tm_hour);
    out->minute = (uint8_t)(utc.tm_min);
    out->second = (uint8_t)(utc.tm_sec);
    out->valid = true;
    return true;
}

static bool rtc_read_utc(meter_wall_time_t *out, void *context)
{
    const rt_tick_t now = rt_tick_get_millisecond();
    /* 无符号差值对 tick 回绕安全；限流窗口内直接复用上一次读数。 */
    if (now - last_poll < METER_RTC_POLL_MS)
    {
        *out = cached_now;
        return cached_valid;
    }
    last_poll = now;
    cached_valid = rtc_sample_utc(out);
    cached_now = *out;
    (void)context;
    return cached_valid;
}

bool meter_rtc_port_available(void)
{
    return rt_device_find("rtc") != RT_NULL;
}

/* 对时落地：civil 读数 → UTC 秒 → RTC 计数器。timegm 只做纯日历换算，不读时区。
   drv_rtc 的 set_secs 会把小于 AIC_RTC_EPOCH 的值整个拒掉，所以调用方必须给出不早于
   2020-05-20T00:00:00Z 的读数。 */
static bool rtc_write_utc(const meter_wall_time_t *utc, void *context)
{
    (void)context;
    if (!meter_rtc_port_available())
        return false;
    /* 用公共层的纯换算而不是 libc 的 timegm：不受 newlib 可见性宏与 32 位时间地平线影响。 */
    int64_t seconds;
    if (!meter_wall_time_to_epoch(utc, &seconds) || seconds < 0 || seconds > (int64_t)INT32_MAX)
        return false;
    if (set_timestamp((time_t)seconds) != RT_EOK)
        return false;
    /* 写成功后立刻刷新读缓存：限流窗口内界面若继续用旧值，用户会以为没写进去。 */
    cached_now = *utc;
    cached_now.valid = true;
    cached_valid = true;
    last_poll = rt_tick_get_millisecond();
    return true;
}

int meter_rtc_port_init(void)
{
#ifdef RT_USING_RTC
    meter_wall_clock_bind(rtc_read_utc, RT_NULL);
    meter_wall_clock_bind_set(rtc_write_utc, RT_NULL);
    if (!meter_rtc_port_available())
    {
        /* 驱动没注册时 time() 仍可能返回 -1，这里只记录一次，避免界面误以为时钟有效。 */
        LOG_E("RTC device missing; wall clock stays unavailable");
        return RT_EOK;
    }
    meter_wall_time_t now;
    if (meter_wall_clock_read(&now))
        LOG_I("RTC ready, UTC %04u-%02u-%02u %02u:%02u:%02u", now.year, now.month, now.day, now.hour,
              now.minute, now.second);
    else
        LOG_W("RTC not set yet; use the `date` shell command to initialize it");
#else
    /* 未启用 RTC 的构建保持未绑定状态，读取一律返回 false，界面显示占位符。 */
    LOG_W("RT_USING_RTC is off; wall clock stays unavailable");
#endif
    return RT_EOK;
}
INIT_APP_EXPORT(meter_rtc_port_init);
