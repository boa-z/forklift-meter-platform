#include "contracts/meter_wall_clock.h"
#include <stdio.h>
#define CHECK(x)                                                                              \
    do                                                                                        \
    {                                                                                         \
        if (!(x))                                                                             \
        {                                                                                     \
            fprintf(stderr, "wall-clock:%d: %s\n", __LINE__, #x);                             \
            return 1;                                                                         \
        }                                                                                     \
    } while (0)
#define ZERO(t) ((t).year == 0u && (t).month == 0u && (t).day == 0u && (t).hour == 0u &&      \
                 (t).minute == 0u && (t).second == 0u && !(t).valid)

typedef struct
{
    meter_wall_time_t value;
    bool ok;
} fake_t;

static bool fake_read(meter_wall_time_t *out, void *context)
{
    const fake_t *fake = context;
    if (!fake->ok)
        return false;
    *out = fake->value;
    return true;
}

typedef struct
{
    meter_wall_time_t written;
    unsigned calls;
    bool ok;
} fake_set_t;

static bool fake_write(const meter_wall_time_t *utc, void *context)
{
    fake_set_t *fake = context;
    ++fake->calls;
    if (!fake->ok)
        return false;
    fake->written = *utc;
    return true;
}

int main(void)
{
    meter_wall_time_t out;

    /* 未绑定即读取：平台端口还没起来时不能给出任何"看起来像 0"的时间。 */
    meter_wall_clock_bind(NULL, NULL);
    out = (meter_wall_time_t){2026u, 10u, 1u, 8u, 30u, 0u, true};
    CHECK(!meter_wall_clock_read(&out) && ZERO(out));
    CHECK(!meter_wall_clock_read(NULL));

    fake_t fake = {.value = {2026u, 10u, 1u, 0u, 30u, 0u, true}, .ok = true};
    meter_wall_clock_bind(fake_read, &fake);
    CHECK(meter_wall_clock_read(&out));
    CHECK(out.year == 2026u && out.month == 10u && out.day == 1u && out.hour == 0u && out.minute == 30u);

    /* 源自己报失败，或报回 valid=false（RTC 掉电复位）都必须被拦住。 */
    fake.ok = false;
    CHECK(!meter_wall_clock_read(&out) && ZERO(out));
    fake.ok = true;
    fake.value.valid = false;
    CHECK(!meter_wall_clock_read(&out) && ZERO(out));
    fake.value.valid = true;

    /* 硬件计数器是外部输入：越界字段一律当不可信，不做静默裁剪。 */
    fake.value.month = 13u;
    CHECK(!meter_wall_clock_read(&out));
    fake.value.month = 10u;
    fake.value.hour = 24u;
    CHECK(!meter_wall_clock_read(&out));
    fake.value.hour = 0u;
    fake.value.year = 1969u;
    CHECK(!meter_wall_clock_read(&out));
    fake.value.year = 2026u;

    /* 平移 +8h：同日内推进。 */
    CHECK(meter_wall_clock_read(&out));
    meter_wall_time_t local;
    CHECK(meter_wall_time_shift(&out, 8 * 3600, &local));
    CHECK(local.day == 1u && local.hour == 8u && local.minute == 30u && local.valid);

    /* 跨过午夜往回：日期跟着退一天。 */
    CHECK(meter_wall_time_shift(&local, -8 * 3600, &local));
    CHECK(local.hour == 0u && local.minute == 30u && local.day == 1u);

    /* 跨过午夜往前：16:05 UTC + 8h 落到次日凌晨。 */
    const meter_wall_time_t evening = {2026u, 10u, 1u, 16u, 5u, 0u, true};
    CHECK(meter_wall_time_shift(&evening, 8 * 3600, &local));
    CHECK(local.day == 2u && local.hour == 0u && local.minute == 5u);

    /* 闰年 2024-02-28 23:00 + 2h 是 2 月 29 日；世纪年 2100 非闰，只能跳到 3 月 1 日。 */
    const meter_wall_time_t leap = {2024u, 2u, 28u, 23u, 0u, 0u, true};
    CHECK(meter_wall_time_shift(&leap, 2 * 3600, &local));
    CHECK(local.month == 2u && local.day == 29u && local.hour == 1u);
    const meter_wall_time_t century = {2100u, 2u, 28u, 23u, 0u, 0u, true};
    CHECK(meter_wall_time_shift(&century, 2 * 3600, &local));
    CHECK(local.month == 3u && local.day == 1u);

    /* 平移把年份推出 1970..9999 时报失败，不猜测回绕值。 */
    const meter_wall_time_t epoch = {1970u, 1u, 1u, 0u, 0u, 0u, true};
    CHECK(!meter_wall_time_shift(&epoch, -3600, &local) && ZERO(local));

    /* 输入不可信时平移同样拒绝，且输出保持清零。 */
    const meter_wall_time_t invalid = {2026u, 10u, 0u, 1u, 1u, 1u, true};
    CHECK(!meter_wall_time_shift(&invalid, 0, &local) && ZERO(local));
    CHECK(!meter_wall_time_shift(NULL, 0, &local));

    /* 连推 24 小时回到同一时刻的次日，累计误差不随时/日换算丢失。 */
    meter_wall_time_t cursor = {2026u, 3u, 1u, 23u, 30u, 45u, true};
    for (unsigned i = 0; i < 24u; ++i)
    {
        CHECK(meter_wall_time_shift(&cursor, 3600, &local));
        cursor = local;
    }
    CHECK(cursor.day == 2u && cursor.hour == 23u && cursor.minute == 30u && cursor.second == 45u);

    /* epoch 换算：写入路径用它构造计数器值，不再依赖 libc 的 timegm。 */
    int64_t seconds, shifted_seconds;
    const meter_wall_time_t origin = {1970u, 1u, 1u, 0u, 0u, 0u, true};
    CHECK(meter_wall_time_to_epoch(&origin, &seconds) && seconds == 0);
    const meter_wall_time_t next_day = {1970u, 1u, 2u, 0u, 0u, 0u, true};
    CHECK(meter_wall_time_to_epoch(&next_day, &seconds) && seconds == 86400);
    /* 2000-01-01T00:00:00Z 是公认常量，用来钉住世纪年附近的换算。 */
    const meter_wall_time_t y2k = {2000u, 1u, 1u, 0u, 0u, 0u, true};
    CHECK(meter_wall_time_to_epoch(&y2k, &seconds) && seconds == INT64_C(946684800));
    /* 闰年与世纪年：2024-02-29 存在，2100-02-29 不存在，因此 2100 只能从 28 跳到 3-01。 */
    const meter_wall_time_t feb28_2024 = {2024u, 2u, 28u, 0u, 0u, 0u, true};
    const meter_wall_time_t feb29_2024 = {2024u, 2u, 29u, 0u, 0u, 0u, true};
    CHECK(meter_wall_time_to_epoch(&feb28_2024, &seconds));
    CHECK(meter_wall_time_to_epoch(&feb29_2024, &shifted_seconds) && shifted_seconds - seconds == 86400);
    const meter_wall_time_t feb28_2100 = {2100u, 2u, 28u, 0u, 0u, 0u, true};
    const meter_wall_time_t mar01_2100 = {2100u, 3u, 1u, 0u, 0u, 0u, true};
    CHECK(meter_wall_time_to_epoch(&feb28_2100, &seconds));
    CHECK(meter_wall_time_to_epoch(&mar01_2100, &shifted_seconds) && shifted_seconds - seconds == 86400);

    /* epoch 与 shift 必须一致：端口写回的计数器值就是平台读回来的那个时刻。 */
    const meter_wall_time_t samples[] = {{2026u, 10u, 1u, 0u, 30u, 0u, true},
                                         {2026u, 12u, 31u, 23u, 59u, 59u, true},
                                         {2024u, 2u, 29u, 12u, 0u, 0u, true}};
    const int offsets[] = {0, 8 * 3600, -5 * 3600};
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
        for (unsigned j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
        {
            CHECK(meter_wall_time_to_epoch(&samples[i], &seconds));
            CHECK(meter_wall_time_shift(&samples[i], offsets[j], &local));
            CHECK(meter_wall_time_to_epoch(&local, &shifted_seconds));
            CHECK(shifted_seconds == seconds + offsets[j]);
        }

    /* 越界读数在换算阶段就被拒掉，输出保持清零。 */
    const meter_wall_time_t unreachable[] = {{1969u, 12u, 31u, 23u, 59u, 59u, true},
                                             {10000u, 1u, 1u, 0u, 0u, 0u, true},
                                             {2026u, 0u, 1u, 0u, 0u, 0u, true}};
    for (unsigned i = 0; i < sizeof(unreachable) / sizeof(unreachable[0]); ++i)
    {
        seconds = 12345;
        CHECK(!meter_wall_time_to_epoch(&unreachable[i], &seconds) && seconds == 0);
    }
    CHECK(!meter_wall_time_to_epoch(NULL, &seconds));
    CHECK(!meter_wall_time_to_epoch(&origin, NULL));

    /* 写通道：没绑定写源的本机（无 RTC 或 RTC 只读）必须报失败，不能假装对时成功。 */    fake_set_t sink = {.ok = true};
    meter_wall_clock_bind_set(NULL, NULL);
    const meter_wall_time_t target = {2026u, 10u, 2u, 1u, 25u, 0u, false};
    CHECK(!meter_wall_clock_write(&target));
    meter_wall_clock_bind_set(fake_write, &sink);
    CHECK(!meter_wall_clock_write(NULL));
    CHECK(sink.calls == 0u);

    /* valid 位在写入侧没有意义（调用方正在构造这个值），只按日历字段放行。 */
    CHECK(meter_wall_clock_write(&target));
    CHECK(sink.calls == 1u && sink.written.hour == 1u && sink.written.minute == 25u);

    /* 越界读数在到达硬件前就拒掉，避免把非法计数写进 RTC。 */
    const meter_wall_time_t bad[] = {{2026u, 13u, 1u, 1u, 0u, 0u, false},
                                     {2026u, 10u, 32u, 1u, 0u, 0u, false},
                                     {2026u, 10u, 1u, 24u, 0u, 0u, false},
                                     {2026u, 10u, 1u, 1u, 60u, 0u, false},
                                     {1969u, 10u, 1u, 1u, 0u, 0u, false},
                                     {2026u, 10u, 1u, 1u, 0u, 60u, false}};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        CHECK(!meter_wall_clock_write(&bad[i]));
        CHECK(sink.calls == 1u);
    }

    /* 硬件写失败要原样上报，界面据此提示"对时未生效"。 */
    sink.ok = false;
    CHECK(!meter_wall_clock_write(&target));
    CHECK(sink.calls == 2u);
    meter_wall_clock_bind_set(NULL, NULL);

    meter_wall_clock_bind(NULL, NULL);
    puts("wall clock PASS");
    return 0;
}
