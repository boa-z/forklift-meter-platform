#define LOG_TAG "meter.settings"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_board_settings.h"
#include "contracts/meter_time.h"
#include <rtdevice.h>
#include <ulog.h>

/* 参考板 PE.17 为 PWM3_B；沿用实板的 50 kHz 与 10% 底座，避免低亮度熄屏。 */
enum { BACKLIGHT_CHANNEL = 3, BACKLIGHT_PERIOD_NS = 20000, BACKLIGHT_RETRY_MS = 1000 };
static uint32_t boot_bitrate = 500000u;
static bool boot_configured, retry_pending;
static uint8_t applied_brightness, requested_brightness;
static uint32_t retry_at;

bool meter_board_settings_boot(const meter_snapshot_t *snapshot)
{
    static const uint32_t rates[] = {125000u, 250000u, 500000u};
    if (boot_configured || !snapshot || (unsigned)snapshot->can_rate >= sizeof(rates) / sizeof(rates[0]))
        return false;
    boot_bitrate = rates[snapshot->can_rate];
    boot_configured = true;
    return true;
}
uint32_t meter_board_can_bitrate(void)
{
    return boot_bitrate;
}
bool meter_board_backlight_apply(uint8_t brightness, uint32_t now_ms)
{
    if (brightness < 10u || brightness > 100u)
        return false;
    if (brightness == applied_brightness && !retry_pending)
        return true;
    if (retry_pending && brightness == requested_brightness && !meter_time_reached(now_ms, retry_at))
        return false;
    requested_brightness = brightness;
    struct rt_device_pwm *device = (struct rt_device_pwm *)rt_device_find("pwm");
    uint32_t pulse = 2000u + (uint32_t)brightness * 180u;
    struct rt_pwm_configuration readback = {.channel = BACKLIGHT_CHANNEL};
    bool ok = device && rt_pwm_set(device, BACKLIGHT_CHANNEL, BACKLIGHT_PERIOD_NS, pulse) == RT_EOK &&
              rt_pwm_enable(device, BACKLIGHT_CHANNEL) == RT_EOK &&
              rt_device_control((rt_device_t)device, PWM_CMD_GET, &readback) == RT_EOK;
    /* 允许硬件时钟量化到相邻纳秒；仍核对真实寄存器读回，不能把 UI 回显视为已应用。 */
    if (ok)
        ok = readback.period >= BACKLIGHT_PERIOD_NS - 100u &&
             readback.period <= BACKLIGHT_PERIOD_NS + 100u &&
             readback.pulse >= pulse - 100u && readback.pulse <= pulse + 100u;
    if (ok)
    {
        applied_brightness = brightness;
        retry_pending = false;
        LOG_I("backlight applied=%u period_ns=%lu pulse_ns=%lu", (unsigned)brightness,
              (unsigned long)readback.period, (unsigned long)readback.pulse);
    }
    else
    {
        if (!retry_pending)
            LOG_E("backlight apply/readback failed; requested=%u; retry_ms=%u",
                  (unsigned)brightness, BACKLIGHT_RETRY_MS);
        retry_pending = true;
        retry_at = now_ms + BACKLIGHT_RETRY_MS;
    }
    return ok;
}
