#ifndef HOST_PLATFORM_H
#define HOST_PLATFORM_H
#include "core/meter_settings.h"
#include <lvgl.h>
bool meter_host_open(bool hidden);
bool meter_host_events(void);
void meter_host_click(int x, int y, bool pressed);
bool meter_host_capture(const char *path);
void meter_host_close(void);
bool meter_host_load(meter_core_t *core, const char *path);
bool meter_host_save(const meter_core_t *core, const char *path);
uint64_t meter_host_counter(void);
double meter_host_us(uint64_t before, uint64_t after);
void meter_host_delay(unsigned ms);
#endif
