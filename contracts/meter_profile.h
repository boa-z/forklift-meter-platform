#ifndef METER_PROFILE_H
#define METER_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
/* Product 定义的稳定系列身份与规范化能力词汇，绝不是原始 CAN 位。
 * 与静态构建能力、传输连接代数分开。零表示未知或未确认；相同内容
 * 的刷新不推进代数。 */
typedef struct
{
    uint32_t generation;
    uint16_t family;
    uint64_t capabilities;
    bool confirmed;
} meter_profile_t;
static inline bool meter_profile_matches(const meter_profile_t *profile, uint32_t generation)
{
    return profile && profile->confirmed && profile->family != 0u && generation != 0u &&
           profile->generation == generation;
}
#endif
