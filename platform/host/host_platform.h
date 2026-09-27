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
/** @brief 读取偏好文件并应用到 core。
 *
 * data 是调用方提供的暂存区，容量不得小于 meter_settings_size()，宿主适配器自身不分配内存。
 * 含阻塞文件 IO，不得在 LVGL 渲染回调中调用；文件缺失或校验不通过时 core 保持原值。
 */
bool meter_host_load(meter_core_t *core, const char *path, uint8_t *data, size_t capacity);
/** @brief 原子写出 core 的偏好：先写临时文件再重命名，读者不会看到半截文件。
 *
 * data 的容量要求与 meter_host_load() 相同。返回 false 表示落盘失败，core 中的新值仍然生效。
 */
bool meter_host_save(const meter_core_t *core, const char *path, uint8_t *data, size_t capacity);
uint64_t meter_host_counter(void);
double meter_host_us(uint64_t before, uint64_t after);
void meter_host_delay(unsigned ms);
#endif
