#ifndef TEST_RTTHREAD_H
#define TEST_RTTHREAD_H
#include <stddef.h>
#include <stdint.h>
#define RT_EOK 0
#define RT_ERROR 1
#define RT_EINVAL 22
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_PRIO 1
struct rt_mutex
{
    const char *name;
    int depth;
};
int rt_mutex_init(struct rt_mutex *, const char *, int);
int rt_mutex_detach(struct rt_mutex *);
int rt_mutex_take(struct rt_mutex *, int);
int rt_mutex_release(struct rt_mutex *);
uint32_t rt_tick_get_millisecond(void);
int rt_kprintf(const char *, ...);
#endif
