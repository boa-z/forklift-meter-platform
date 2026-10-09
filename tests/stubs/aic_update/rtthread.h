#pragma once
#include <rtconfig.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ALIGN(n) __attribute__((aligned(n)))
#define RT_EOK 0
#define RT_ERROR 1
#define RT_ENOSYS 38
typedef int rt_err_t;
#define RT_NULL NULL
#define RT_Device_Class_MTD 1
#define RT_DEVICE_OFLAG_RDONLY 1
#define rt_malloc malloc
#define rt_free free
#define rt_memcpy memcpy
#define rt_strncmp strncmp
struct rt_device
{
    int type;
};
typedef struct rt_device *rt_device_t;
rt_device_t rt_device_find(const char *name);
int rt_device_open(rt_device_t device, int flags);
int rt_device_close(rt_device_t device);
