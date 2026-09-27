/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include <string.h>

#include "product.h"

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

int product_drivetrain_pack(
    uint8_t *dst_p,
    const struct product_drivetrain_t *src_p,
    size_t size)
{
    uint16_t torque;

    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u16(src_p->velocity, 0u, 0xffu);
    dst_p[1] |= pack_right_shift_u16(src_p->velocity, 8u, 0xffu);
    torque = (uint16_t)src_p->torque;
    dst_p[2] |= pack_left_shift_u16(torque, 0u, 0xffu);
    dst_p[3] |= pack_right_shift_u16(torque, 8u, 0xffu);

    return (8);
}

int product_drivetrain_unpack(
    struct product_drivetrain_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    uint16_t torque;

    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->velocity = unpack_right_shift_u16(src_p[0], 0u, 0xffu);
    dst_p->velocity |= unpack_left_shift_u16(src_p[1], 8u, 0xffu);
    torque = unpack_right_shift_u16(src_p[2], 0u, 0xffu);
    torque |= unpack_left_shift_u16(src_p[3], 8u, 0xffu);
    dst_p->torque = (int16_t)torque;

    return (0);
}

int product_drivetrain_init(struct product_drivetrain_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct product_drivetrain_t));

    return 0;
}

uint16_t product_drivetrain_velocity_encode(float value)
{
    return (uint16_t)(value / 0.1f);
}

float product_drivetrain_velocity_decode(uint16_t value)
{
    return ((float)value * 0.1f);
}

bool product_drivetrain_velocity_is_in_range(uint16_t value)
{
    return (value <= 120u);
}

int16_t product_drivetrain_torque_encode(float value)
{
    return (int16_t)(value / 0.5f);
}

float product_drivetrain_torque_decode(int16_t value)
{
    return ((float)value * 0.5f);
}

bool product_drivetrain_torque_is_in_range(int16_t value)
{
    return ((value >= -200) && (value <= 200));
}

int product_power_pack(
    uint8_t *dst_p,
    const struct product_power_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u8(src_p->remaining, 0u, 0xffu);
    dst_p[1] |= pack_left_shift_u16(src_p->ambient, 0u, 0xffu);
    dst_p[2] |= pack_right_shift_u16(src_p->ambient, 8u, 0xffu);

    return (8);
}

int product_power_unpack(
    struct product_power_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->remaining = unpack_right_shift_u8(src_p[0], 0u, 0xffu);
    dst_p->ambient = unpack_right_shift_u16(src_p[1], 0u, 0xffu);
    dst_p->ambient |= unpack_left_shift_u16(src_p[2], 8u, 0xffu);

    return (0);
}

int product_power_init(struct product_power_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct product_power_t));

    return 0;
}

uint8_t product_power_remaining_encode(float value)
{
    return (uint8_t)(value / 0.5f);
}

float product_power_remaining_decode(uint8_t value)
{
    return ((float)value * 0.5f);
}

bool product_power_remaining_is_in_range(uint8_t value)
{
    return (value <= 200u);
}

uint16_t product_power_ambient_encode(float value)
{
    return (uint16_t)((value - -40.0f) / 0.1f);
}

float product_power_ambient_decode(uint16_t value)
{
    return (((float)value * 0.1f) + -40.0f);
}

bool product_power_ambient_is_in_range(uint16_t value)
{
    return (value <= 1600u);
}
