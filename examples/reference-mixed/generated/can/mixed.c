/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include <string.h>

#include "mixed.h"

static inline uint8_t pack_left_shift_u8(
    uint8_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint8_t)((uint8_t)(value << shift) & mask);
}

static inline uint8_t pack_left_shift_u16(
    uint16_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint8_t)((uint8_t)(value << shift) & mask);
}

static inline uint8_t pack_right_shift_u16(
    uint16_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint8_t)((uint8_t)(value >> shift) & mask);
}

static inline uint16_t unpack_left_shift_u16(
    uint8_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint16_t)((uint16_t)(value & mask) << shift);
}

static inline uint8_t unpack_right_shift_u8(
    uint8_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint8_t)((uint8_t)(value & mask) >> shift);
}

static inline uint16_t unpack_right_shift_u16(
    uint8_t value,
    uint8_t shift,
    uint8_t mask)
{
    return (uint16_t)((uint16_t)(value & mask) >> shift);
}

int mixed_motion_pack(
    uint8_t *dst_p,
    const struct mixed_motion_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u16(src_p->speed, 0u, 0xffu);
    dst_p[1] |= pack_right_shift_u16(src_p->speed, 8u, 0xffu);

    return (8);
}

int mixed_motion_unpack(
    struct mixed_motion_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->speed = unpack_right_shift_u16(src_p[0], 0u, 0xffu);
    dst_p->speed |= unpack_left_shift_u16(src_p[1], 8u, 0xffu);

    return (0);
}

int mixed_motion_init(struct mixed_motion_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct mixed_motion_t));

    return 0;
}

uint16_t mixed_motion_speed_encode(float value)
{
    return (uint16_t)(value / 0.1f);
}

float mixed_motion_speed_decode(uint16_t value)
{
    return ((float)value * 0.1f);
}

bool mixed_motion_speed_is_in_range(uint16_t value)
{
    return (value <= 500u);
}

int mixed_energy_pack(
    uint8_t *dst_p,
    const struct mixed_energy_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u8(src_p->soc, 0u, 0xffu);

    return (8);
}

int mixed_energy_unpack(
    struct mixed_energy_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->soc = unpack_right_shift_u8(src_p[0], 0u, 0xffu);

    return (0);
}

int mixed_energy_init(struct mixed_energy_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct mixed_energy_t));

    return 0;
}

uint8_t mixed_energy_soc_encode(float value)
{
    return (uint8_t)(value / 0.5f);
}

float mixed_energy_soc_decode(uint8_t value)
{
    return ((float)value * 0.5f);
}

bool mixed_energy_soc_is_in_range(uint8_t value)
{
    return (value <= 200u);
}
