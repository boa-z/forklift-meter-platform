#pragma once
#include <boot_param.h>
#include <rtthread.h>
typedef uint8_t u8;
#define CACHE_LINE_SIZE 64
#define aicos_malloc_align(a, b, c) malloc(b)
#define aicos_free_align(a, b) free(b)
#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) (x)
