#define LOG_TAG "hmi.settings"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/hmi_board_settings.h"
#include "contracts/hmi_time.h"
#include <rtdevice.h>
#include <ulog.h>

static uint32_t boot_bitrate = 500000u;
static bool boot_configured;

bool hmi_board_settings_boot(const hmi_snapshot_t *snapshot)
{
    static const uint32_t rates[] = {125000u, 250000u, 500000u};
    if (boot_configured || !snapshot || (unsigned)snapshot->can_rate >= sizeof(rates) / sizeof(rates[0]))
        return false;
    boot_bitrate = rates[snapshot->can_rate];
    boot_configured = true;
    return true;
}
uint32_t hmi_board_can_bitrate(void)
{
    return boot_bitrate;
}
#ifdef AIC_HMI_BACKLIGHT
/* 通道来自 SDK 板级配置 AIC_PWM_BACKLIGHT_CHANNEL；周期与底座占空比来自 Kconfig。 */
#ifndef AIC_HMI_BACKLIGHT_PERIOD_NS
#define AIC_HMI_BACKLIGHT_PERIOD_NS 20000
#endif
#ifndef AIC_HMI_BACKLIGHT_FLOOR_PERCENT
#define AIC_HMI_BACKLIGHT_FLOOR_PERCENT 10
#endif
enum
{
    BACKLIGHT_PERIOD_NS = AIC_HMI_BACKLIGHT_PERIOD_NS,
    BACKLIGHT_FLOOR = AIC_HMI_BACKLIGHT_FLOOR_PERCENT,
    BACKLIGHT_RETRY_MS = 1000
};
static bool retry_pending;
static uint8_t applied_brightness, requested_brightness;
static uint32_t retry_at;
bool hmi_board_backlight_apply(uint8_t brightness, uint32_t now_ms)
{
    if (brightness < 10u || brightness > 100u)
        return false;
    if (brightness == applied_brightness && !retry_pending)
        return true;
    if (retry_pending && brightness == requested_brightness && !hmi_time_reached(now_ms, retry_at))
        return false;
    requested_brightness = brightness;
    struct rt_device_pwm *device = (struct rt_device_pwm *)rt_device_find("pwm");
    const uint32_t period = (uint32_t)BACKLIGHT_PERIOD_NS;
    const uint32_t pulse = period * (uint32_t)BACKLIGHT_FLOOR / 100u +
                           (uint32_t)((uint64_t)period * (100u - (uint32_t)BACKLIGHT_FLOOR) * brightness / 10000u);
    struct rt_pwm_configuration readback = {.channel = AIC_PWM_BACKLIGHT_CHANNEL};
    bool ok = device && rt_pwm_set(device, AIC_PWM_BACKLIGHT_CHANNEL, period, pulse) == RT_EOK &&
              rt_pwm_enable(device, AIC_PWM_BACKLIGHT_CHANNEL) == RT_EOK &&
              rt_device_control((rt_device_t)device, PWM_CMD_GET, &readback) == RT_EOK;
    /* 允许硬件时钟量化到相邻纳秒；仍核对真实寄存器读回，不能把 UI 回显视为已应用。 */
    if (ok)
        ok = readback.period >= period - 100u && readback.period <= period + 100u && readback.pulse + 100u >= pulse &&
             readback.pulse <= pulse + 100u;
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
            LOG_E("backlight apply/readback failed; requested=%u; retry_ms=%u", (unsigned)brightness,
                  BACKLIGHT_RETRY_MS);
        retry_pending = true;
        retry_at = now_ms + BACKLIGHT_RETRY_MS;
    }
    return ok;
}
#else
bool hmi_board_backlight_apply(uint8_t brightness, uint32_t now_ms)
{
    /* 背光由 SDK 默认值或 Product 自己负责；设置值仍照常保存。 */
    (void)now_ms;
    return brightness >= 10u && brightness <= 100u;
}
#endif
