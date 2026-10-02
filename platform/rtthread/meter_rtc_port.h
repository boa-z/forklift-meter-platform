#ifndef METER_RTC_PORT_H
#define METER_RTC_PORT_H
#include <stdbool.h>

/** @brief 把板载 RTC 绑定为全局墙上时钟来源；由 INIT_APP_EXPORT 自动调用，也可重复调用换源。 */
int meter_rtc_port_init(void);

/** @brief 板载 RTC 是否已被驱动注册（不代表时间已设置）。 */
bool meter_rtc_port_available(void);

/**
 * @brief 可信读数的下界（UTC 秒）；小于等于该值的读数按"未设置"处理。
 *
 * 驱动内部有私有纪元：d13x 的 RTC 只是 32 位秒计数器，drv_rtc 给它加上
 * AIC_RTC_EPOCH(1589932800，2020-05-20T00:00:00Z) 才是 UTC 秒。掉电且无备份电池时
 * 计数器归零，第一秒的读数正好等于这个哨兵值，软件无法与"真的就是这一秒"区分。
 *
 * 有备份电池或每上电都会对时的产品，应把这个下界提高到自己的生产日期，
 * 这样未初始化的计数器会被稳定拒绝，而不是显示成 2020-05-20。
 */
#ifndef METER_RTC_FLOOR_UTC
#define METER_RTC_FLOOR_UTC 1589932800LL
#endif

/** @brief 读缓存刷新周期；界面可见分辨率是分钟，因此不需要每帧访问驱动。 */
#ifndef METER_RTC_POLL_MS
#define METER_RTC_POLL_MS 250u
#endif

#endif
