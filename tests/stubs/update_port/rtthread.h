#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RT_EOK 0
#define RT_NULL NULL
#define RT_WAITING_FOREVER (-1)
#define RT_EVENT_FLAG_OR 1
#define RT_EVENT_FLAG_CLEAR 2
#define RT_IPC_FLAG_PRIO 0
#define RT_IPC_FLAG_FIFO 0
typedef uintptr_t rt_ubase_t;
typedef uint32_t rt_uint32_t;
struct rt_thread
{
    int unused;
};
struct rt_mutex
{
    bool held;
};
struct rt_event
{
    unsigned bits;
};
struct rt_messagequeue
{
    bool full;
    size_t size;
    uint8_t data[1024];
};
int rt_mutex_init(struct rt_mutex *, const char *, int);
int rt_mutex_take(struct rt_mutex *, int);
int rt_mutex_release(struct rt_mutex *);
int rt_mq_init(struct rt_messagequeue *, const char *, void *, size_t, size_t, int);
int rt_mq_send(struct rt_messagequeue *, const void *, size_t);
int rt_mq_recv(struct rt_messagequeue *, void *, size_t, int);
int rt_event_init(struct rt_event *, const char *, int);
int rt_event_recv(struct rt_event *, unsigned, int, int, rt_uint32_t *);
int rt_event_send(struct rt_event *, unsigned);
int rt_thread_init(struct rt_thread *, const char *, void (*)(void *), void *, void *, size_t, int, int);
int rt_thread_startup(struct rt_thread *);
int rt_tick_from_millisecond(int);
uint32_t rt_tick_get_millisecond(void);
void rt_thread_mdelay(int);
int rt_kprintf(const char *, ...);
