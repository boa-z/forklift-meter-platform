#ifndef HOST_PLATFORM_H
#define HOST_PLATFORM_H
#include "core/meter_settings.h"
#include <lvgl.h>
/* 窗口与输入接口只由持有 LVGL 的唯一 UI 线程调用；
 * 其他线程并发访问 SDL 或 lv_timer_handler() 都不安全。 */
bool meter_host_open(bool hidden);
bool meter_host_events(void);
void meter_host_click(int x, int y, bool pressed);
bool meter_host_capture(const char *path);
void meter_host_close(void);
uint64_t meter_host_counter(void);
double meter_host_us(uint64_t before, uint64_t after);
void meter_host_delay(unsigned ms);
#endif
