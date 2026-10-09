#include "contracts/meter_wall_clock.h"
#include <stddef.h>

/* 绑定只发生在启动阶段（平台 INIT_*），运行期只有读取，因此不需要锁；
   重复绑定允许，用于换源或测试注入。 */
static meter_wall_clock_fn_t source;
static void *source_context;
static meter_wall_clock_set_fn_t sink;
static void *sink_context;

/* 以 1970-01-01 为原点的 civil<->day 换算（Howard Hinnant 算法），
   不依赖 newlib 的时区数据，也不受 32 位 time_t 的年份限制。 */
static int days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2u;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2u ? (unsigned)-3u : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int)doe - 719468;
}

static void civil_from_days(int z, int *y, unsigned *m, unsigned *d)
{
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    *d = doy - (153u * mp + 2u) / 5u + 1u;
    *m = mp + (mp < 10u ? 3u : (unsigned)-9u);
    *y = (int)yoe + era * 400 + (int)(*m <= 2u);
}

static bool civil_in_range(const meter_wall_time_t *t)
{
    return t->year >= 1970u && t->year <= 9999u && t->month >= 1u && t->month <= 12u &&
           t->day >= 1u && t->day <= 31u && t->hour < 24u && t->minute < 60u && t->second < 60u;
}

void meter_wall_clock_bind(meter_wall_clock_fn_t read, void *context)
{
    source = read;
    source_context = context;
}

bool meter_wall_clock_read(meter_wall_time_t *utc_out)
{
    if (!utc_out)
        return false;
    *utc_out = (meter_wall_time_t){0u, 0u, 0u, 0u, 0u, 0u, false};
    if (!source)
        return false;
    meter_wall_time_t sample = {0u, 0u, 0u, 0u, 0u, 0u, false};
    /* 硬件计数器是外部来源：越界读数按不可信处理，不让它进入界面。 */
    if (!source(&sample, source_context) || !sample.valid || !civil_in_range(&sample))
        return false;
    sample.valid = true;
    *utc_out = sample;
    return true;
}

void meter_wall_clock_bind_set(meter_wall_clock_set_fn_t write, void *context)
{
    sink = write;
    sink_context = context;
}

bool meter_wall_clock_write(const meter_wall_time_t *utc)
{
    /* 写入的是调用方刚构造的读数，valid 位没有意义，因此只校验日历字段本身。 */
    if (!sink || !utc || !civil_in_range(utc))
        return false;
    return sink(utc, sink_context);
}

bool meter_wall_time_to_epoch(const meter_wall_time_t *utc, int64_t *seconds_out)
{
    if (!utc || !seconds_out || !civil_in_range(utc))
    {
        if (seconds_out)
            *seconds_out = 0;
        return false;
    }
    *seconds_out = (int64_t)days_from_civil((int)utc->year, utc->month, utc->day) * INT64_C(86400) +
                   (int64_t)utc->hour * INT64_C(3600) + (int64_t)utc->minute * INT64_C(60) + utc->second;
    return true;
}

bool meter_wall_time_shift(const meter_wall_time_t *utc, int offset_seconds, meter_wall_time_t *local_out)
{
    if (!utc || !local_out || !utc->valid || !civil_in_range(utc))
    {
        if (local_out)
            *local_out = (meter_wall_time_t){0u, 0u, 0u, 0u, 0u, 0u, false};
        return false;
    }
    const long long seconds = (long long)days_from_civil((int)utc->year, utc->month, utc->day) * 86400LL +
                              (long long)utc->hour * 3600LL + (long long)utc->minute * 60LL + utc->second +
                              offset_seconds;
    long long day_floor = seconds / 86400LL;
    long long day_second = seconds - day_floor * 86400LL;
    if (day_second < 0)
    {
        day_second += 86400LL;
        --day_floor;
    }
    int year;
    unsigned month, day;
    civil_from_days((int)day_floor, &year, &month, &day);
    meter_wall_time_t shifted = {(uint16_t)year, (uint8_t)month, (uint8_t)day,
                                 (uint8_t)(day_second / 3600LL), (uint8_t)((day_second / 60LL) % 60LL),
                                 (uint8_t)(day_second % 60LL), true};
    /* 平移把日期推出可表示范围时不猜测回绕值。 */
    if (year < 1970 || year > 9999 || !civil_in_range(&shifted))
    {
        *local_out = (meter_wall_time_t){0u, 0u, 0u, 0u, 0u, 0u, false};
        return false;
    }
    *local_out = shifted;
    return true;
}
