#pragma once
#include <stddef.h>
#include <stdint.h>
#define RT_EOK 0
#define RT_ERROR 1
#define RT_WAITING_FOREVER (-1)
#define RT_NULL NULL
#define RT_IPC_FLAG_PRIO 0
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_CMD_RESET 0
#define RT_ALIGN_SIZE sizeof(uintptr_t)
#define RT_ALIGN(n,a) (((n)+(a)-1u)&~((a)-1u))
typedef uintptr_t rt_ubase_t;
typedef intptr_t rt_base_t;
struct rt_mutex { unsigned depth; };
struct rt_semaphore { unsigned value; };
struct rt_thread { unsigned unused; };
struct rt_messagequeue { unsigned entry, head, tail, limit; size_t size; uint8_t data[64][1024]; };
int rt_mutex_init(struct rt_mutex *, const char *, int);
int rt_mutex_take(struct rt_mutex *, int);
int rt_mutex_release(struct rt_mutex *);
int rt_sem_init(struct rt_semaphore *, const char *, unsigned, int);
int rt_sem_release(struct rt_semaphore *);
int rt_sem_take(struct rt_semaphore *, int);
int rt_sem_control(struct rt_semaphore *, int, void *);
int rt_mq_init(struct rt_messagequeue *, const char *, void *, size_t, size_t, int);
int rt_mq_send(struct rt_messagequeue *, const void *, size_t);
int rt_mq_recv(struct rt_messagequeue *, void *, size_t, int);
int rt_thread_init(struct rt_thread *, const char *, void (*)(void *), void *, void *, size_t, int, int);
int rt_thread_startup(struct rt_thread *);
void rt_thread_mdelay(int);
int rt_tick_from_millisecond(int);
uint32_t rt_tick_get_millisecond(void);
int rt_kprintf(const char *, ...);
