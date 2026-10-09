/* 使用真实适配代码与设备替身，覆盖失败重试、寄存器读回和启动后配置不可变。 */
#include "platform/rtthread/meter_board_settings.c"
#include <assert.h>
#include <string.h>
static struct rt_device_pwm pwm;
static unsigned calls, failure;
static uint32_t last_period, last_pulse;
rt_device_t rt_device_find(const char *name)
{
    assert(!strcmp(name, "pwm"));
    ++calls;
    return failure == 1 ? NULL : &pwm;
}
int rt_pwm_set(struct rt_device_pwm *device, int channel, uint32_t period, uint32_t pulse)
{
    assert(device == &pwm && channel == 3);
    last_period = period; last_pulse = pulse;
    return failure == 2 ? -1 : 0;
}
int rt_pwm_enable(struct rt_device_pwm *device, int channel)
{
    assert(device == &pwm && channel == 3);
    return failure == 3 ? -1 : 0;
}
int rt_device_control(rt_device_t device, int command, void *value)
{
    assert(device == &pwm && command == PWM_CMD_GET);
    struct rt_pwm_configuration *readback = value;
    assert(readback->channel == 3);
    readback->period = last_period;
    readback->pulse = failure == 5 ? 0 : last_pulse;
    return failure == 4 ? -1 : 0;
}
int main(void)
{
    meter_snapshot_t snapshot = {.can_rate = (meter_can_rate_t)3};
    assert(!meter_board_settings_boot(NULL) && !meter_board_settings_boot(&snapshot));
    snapshot.can_rate = METER_CAN_RATE_250K;
    assert(meter_board_settings_boot(&snapshot) && meter_board_can_bitrate() == 250000u);
    snapshot.can_rate = METER_CAN_RATE_125K;
    assert(!meter_board_settings_boot(&snapshot) && meter_board_can_bitrate() == 250000u);
    assert(!meter_board_backlight_apply(9, 0) && !meter_board_backlight_apply(101, 0) && calls == 0);
    assert(meter_board_backlight_apply(10, 0) && last_period == 20000u && last_pulse == 3800u);
    assert(meter_board_backlight_apply(10, 1) && calls == 1);
    assert(meter_board_backlight_apply(100, 2) && last_pulse == 20000u);
    for (failure = 1; failure <= 5; ++failure)
    {
        uint32_t now = failure * 2000u;
        assert(!meter_board_backlight_apply((uint8_t)(20 + failure), now));
        unsigned attempts = calls;
        assert(!meter_board_backlight_apply((uint8_t)(20 + failure), now + 999u));
        assert(calls == attempts);
        assert(!meter_board_backlight_apply((uint8_t)(20 + failure), now + 1000u));
        assert(calls == attempts + 1u);
    }
    failure = 0;
    assert(meter_board_backlight_apply(80, 13000) && last_pulse == 16400u);
    failure = 1;
    assert(!meter_board_backlight_apply(70, UINT32_MAX - 500u));
    unsigned attempts = calls;
    assert(!meter_board_backlight_apply(70, 498) && calls == attempts);
    failure = 0;
    assert(meter_board_backlight_apply(70, 499) && calls == attempts + 1u);
    return 0;
}
