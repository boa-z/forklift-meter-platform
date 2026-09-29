#ifndef TEST_SETTINGS_RTDEVICE_H
#define TEST_SETTINGS_RTDEVICE_H
#include <stdint.h>
#define RT_EOK 0
#define PWM_CMD_GET 3
struct rt_device_pwm { int unused; };
typedef struct rt_device_pwm *rt_device_t;
struct rt_pwm_configuration { int channel; uint32_t period, pulse; };
rt_device_t rt_device_find(const char *name);
int rt_pwm_set(struct rt_device_pwm *device, int channel, uint32_t period, uint32_t pulse);
int rt_pwm_enable(struct rt_device_pwm *device, int channel);
int rt_device_control(rt_device_t device, int command, void *value);
#endif
