#ifndef TEST_EEPROM_RTTHREAD_H
#define TEST_EEPROM_RTTHREAD_H
#include <stddef.h>
#include <stdint.h>
typedef size_t rt_size_t;
typedef int rt_err_t;
typedef int32_t rt_int32_t;
typedef uint32_t rt_uint32_t;
#define RT_EOK 0
#define RT_WAITING_FOREVER (-1)
struct rt_mutex
{
    unsigned depth;
};
rt_err_t rt_mutex_take(struct rt_mutex *mutex, rt_int32_t timeout);
rt_err_t rt_mutex_release(struct rt_mutex *mutex);
rt_err_t rt_thread_mdelay(rt_int32_t ms);
#endif
