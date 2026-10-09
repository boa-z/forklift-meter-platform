#ifndef METER_TIME_H
#define METER_TIME_H
#include <stdbool.h>
#include <stdint.h>

/** @brief RT-CAN-05：单调毫秒期限的半周期范围，最大合法间隔为此值减一。 */
#define METER_TIME_HALF_RANGE UINT32_C(0x80000000)

/**
 * @brief 任意 owner 可调用的纯函数；不阻塞、不持有对象、不适用于墙上时钟。
 * @param now_ms 当前单调毫秒，允许无符号回绕。
 * @param deadline_ms 已排定期限；调用必须距期限小于半周期，否则无法判别先后。
 * @return 已到期为 true；精确半周期返回 false。无取消或状态改变。
 */
static inline bool meter_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < METER_TIME_HALF_RANGE;
}
#endif
