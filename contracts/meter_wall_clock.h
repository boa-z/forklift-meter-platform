#ifndef METER_WALL_CLOCK_H
#define METER_WALL_CLOCK_H
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 墙上时钟读数（civil 时间），不是单调毫秒。
 *
 * valid 为 false 表示时钟不可信：本机没有 RTC、驱动未注册，或计数器仍停在
 * 掉电复位值上。单调期限计算请继续用 contracts/meter_time.h，两者不可互换。
 */
typedef struct
{
    uint16_t year;                 /**< 公历年，1970..9999 */
    uint8_t month, day;            /**< 月 1..12，日 1..31 */
    uint8_t hour, minute, second;  /**< 时 0..23，分秒 0..59 */
    bool valid;                    /**< 读数为真时才有意义 */
} meter_wall_time_t;

/** @brief 平台读取回调：填入 UTC 读数并返回 true；读不到返回 false。不得阻塞或做设备 I/O。 */
typedef bool (*meter_wall_clock_fn_t)(meter_wall_time_t *utc_out, void *context);

/** @brief 平台在启动阶段绑定唯一的墙上时钟来源；传 NULL 表示本机没有可用时钟。 */
void meter_wall_clock_bind(meter_wall_clock_fn_t read, void *context);

/** @brief 读取 UTC 墙上时间。未绑定或来源失败时 *utc_out 归零且 valid=false，返回 false。 */
bool meter_wall_clock_read(meter_wall_time_t *utc_out);

/** @brief 平台写入回调：把 UTC 读数落到本机计数器；写不进返回 false。不得阻塞。 */
typedef bool (*meter_wall_clock_set_fn_t)(const meter_wall_time_t *utc, void *context);

/** @brief 启动阶段绑定可选的写入通道；不绑定或传 NULL 表示本机时钟不可设置。 */
void meter_wall_clock_bind_set(meter_wall_clock_set_fn_t write, void *context);

/**
 * @brief 把 UTC 读数写回本机时钟。
 *
 * 与读取不同，这里不要求 utc->valid（调用方正在构造这个值），但日历字段越界一律拒绝，
 * 不向硬件下发猜测值。未绑定写通道或硬件写入失败返回 false，调用方据此提示用户。
 */
bool meter_wall_clock_write(const meter_wall_time_t *utc);

/**
 * @brief 纯函数：把 UTC civil 读数按固定秒数平移成显示区域的 civil 时间。
 *
 * 只做日历换算，不查时区表，因此公共层不需要 OS 的时区数据；跨日、跨月、
 * 闰年和负偏移都由本函数处理。年份落在 1970..9999 之外视为无效并返回 false。
 * @param utc 输入的 UTC 读数，valid 必须为真。
 * @param offset_seconds 区域相对 UTC 的秒偏移，例如东八区为 8*3600。
 * @param local_out 输出平移后的本地读数。
 */
bool meter_wall_time_shift(const meter_wall_time_t *utc, int offset_seconds, meter_wall_time_t *local_out);

/**
 * @brief 纯函数：把 UTC civil 读数换算成 1970-01-01T00:00:00Z 起的秒数。
 *
 * 与写入路径配套：平台端口需要的是"计数器该写多少"，而这个换算不该依赖
 * libc 的 timegm，也不该受 32 位 time_t 的地平线限制。只校验日历字段是否合理，
 * 不要求 utc->valid——调用方正在构造一个待写入的值。越界或 NULL 时清零输出并返回 false。
 */
bool meter_wall_time_to_epoch(const meter_wall_time_t *utc, int64_t *seconds_out);
#endif
