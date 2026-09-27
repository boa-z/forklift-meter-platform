/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#include <string.h>

#include "demo.h"

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

int demo_motion_pack(
    uint8_t *dst_p,
    const struct demo_motion_t *src_p,
    size_t size)
{
    uint16_t steering;

    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u16(src_p->speed, 0u, 0xffu);
    dst_p[1] |= pack_right_shift_u16(src_p->speed, 8u, 0xffu);
    steering = (uint16_t)src_p->steering;
    dst_p[2] |= pack_left_shift_u16(steering, 0u, 0xffu);
    dst_p[3] |= pack_right_shift_u16(steering, 8u, 0xffu);
    dst_p[4] |= pack_left_shift_u16(src_p->hours, 0u, 0xffu);
    dst_p[5] |= pack_right_shift_u16(src_p->hours, 8u, 0xffu);

    return (8);
}

int demo_motion_unpack(
    struct demo_motion_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    uint16_t steering;

    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->speed = unpack_right_shift_u16(src_p[0], 0u, 0xffu);
    dst_p->speed |= unpack_left_shift_u16(src_p[1], 8u, 0xffu);
    steering = unpack_right_shift_u16(src_p[2], 0u, 0xffu);
    steering |= unpack_left_shift_u16(src_p[3], 8u, 0xffu);
    dst_p->steering = (int16_t)steering;
    dst_p->hours = unpack_right_shift_u16(src_p[4], 0u, 0xffu);
    dst_p->hours |= unpack_left_shift_u16(src_p[5], 8u, 0xffu);

    return (0);
}

int demo_motion_init(struct demo_motion_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct demo_motion_t));

    return 0;
}

uint16_t demo_motion_speed_encode(float value)
{
    return (uint16_t)(value / 0.01f);
}

float demo_motion_speed_decode(uint16_t value)
{
    return ((float)value * 0.01f);
}

bool demo_motion_speed_is_in_range(uint16_t value)
{
    return (value <= 5000u);
}

int16_t demo_motion_steering_encode(float value)
{
    return (int16_t)(value / 0.01f);
}

float demo_motion_steering_decode(int16_t value)
{
    return ((float)value * 0.01f);
}

bool demo_motion_steering_is_in_range(int16_t value)
{
    return ((value >= -4500) && (value <= 4500));
}

uint16_t demo_motion_hours_encode(float value)
{
    return (uint16_t)(value / 0.1f);
}

float demo_motion_hours_decode(uint16_t value)
{
    return ((float)value * 0.1f);
}

bool demo_motion_hours_is_in_range(uint16_t value)
{
    (void)value;

    return (true);
}

int demo_energy_pack(
    uint8_t *dst_p,
    const struct demo_energy_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u8(src_p->soc, 0u, 0xffu);
    dst_p[1] |= pack_left_shift_u16(src_p->voltage, 0u, 0xffu);
    dst_p[2] |= pack_right_shift_u16(src_p->voltage, 8u, 0xffu);
    dst_p[3] |= pack_left_shift_u8(src_p->charging, 0u, 0x01u);

    return (8);
}

int demo_energy_unpack(
    struct demo_energy_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->soc = unpack_right_shift_u8(src_p[0], 0u, 0xffu);
    dst_p->voltage = unpack_right_shift_u16(src_p[1], 0u, 0xffu);
    dst_p->voltage |= unpack_left_shift_u16(src_p[2], 8u, 0xffu);
    dst_p->charging = unpack_right_shift_u8(src_p[3], 0u, 0x01u);

    return (0);
}

int demo_energy_init(struct demo_energy_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct demo_energy_t));

    return 0;
}

uint8_t demo_energy_soc_encode(float value)
{
    return (uint8_t)(value);
}

float demo_energy_soc_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_energy_soc_is_in_range(uint8_t value)
{
    return (value <= 100u);
}

uint16_t demo_energy_voltage_encode(float value)
{
    return (uint16_t)(value / 0.01f);
}

float demo_energy_voltage_decode(uint16_t value)
{
    return ((float)value * 0.01f);
}

bool demo_energy_voltage_is_in_range(uint16_t value)
{
    return (value <= 10000u);
}

uint8_t demo_energy_charging_encode(float value)
{
    return (uint8_t)(value);
}

float demo_energy_charging_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_energy_charging_is_in_range(uint8_t value)
{
    return (value <= 1u);
}

int demo_lift_pack(
    uint8_t *dst_p,
    const struct demo_lift_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u16(src_p->height, 0u, 0xffu);
    dst_p[1] |= pack_right_shift_u16(src_p->height, 8u, 0xffu);

    return (8);
}

int demo_lift_unpack(
    struct demo_lift_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->height = unpack_right_shift_u16(src_p[0], 0u, 0xffu);
    dst_p->height |= unpack_left_shift_u16(src_p[1], 8u, 0xffu);

    return (0);
}

int demo_lift_init(struct demo_lift_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct demo_lift_t));

    return 0;
}

uint16_t demo_lift_height_encode(float value)
{
    return (uint16_t)(value / 0.001f);
}

float demo_lift_height_decode(uint16_t value)
{
    return ((float)value * 0.001f);
}

bool demo_lift_height_is_in_range(uint16_t value)
{
    return (value <= 6000u);
}

int demo_load_pack(
    uint8_t *dst_p,
    const struct demo_load_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u16(src_p->weight, 0u, 0xffu);
    dst_p[1] |= pack_right_shift_u16(src_p->weight, 8u, 0xffu);

    return (8);
}

int demo_load_unpack(
    struct demo_load_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->weight = unpack_right_shift_u16(src_p[0], 0u, 0xffu);
    dst_p->weight |= unpack_left_shift_u16(src_p[1], 8u, 0xffu);

    return (0);
}

int demo_load_init(struct demo_load_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct demo_load_t));

    return 0;
}

uint16_t demo_load_weight_encode(float value)
{
    return (uint16_t)(value);
}

float demo_load_weight_decode(uint16_t value)
{
    return ((float)value);
}

bool demo_load_weight_is_in_range(uint16_t value)
{
    return (value <= 1500u);
}

int demo_status_pack(
    uint8_t *dst_p,
    const struct demo_status_t *src_p,
    size_t size)
{
    uint16_t controller_temp;
    uint16_t motor_temp;

    if (size < 8u) {
        return (-EINVAL);
    }

    memset(&dst_p[0], 0, 8);

    dst_p[0] |= pack_left_shift_u8(src_p->seat, 0u, 0x01u);
    dst_p[0] |= pack_left_shift_u8(src_p->brake, 1u, 0x02u);
    dst_p[0] |= pack_left_shift_u8(src_p->neutral, 2u, 0x04u);
    dst_p[0] |= pack_left_shift_u8(src_p->warning, 3u, 0x08u);
    motor_temp = (uint16_t)src_p->motor_temp;
    dst_p[1] |= pack_left_shift_u16(motor_temp, 0u, 0xffu);
    dst_p[2] |= pack_right_shift_u16(motor_temp, 8u, 0xffu);
    controller_temp = (uint16_t)src_p->controller_temp;
    dst_p[3] |= pack_left_shift_u16(controller_temp, 0u, 0xffu);
    dst_p[4] |= pack_right_shift_u16(controller_temp, 8u, 0xffu);

    return (8);
}

int demo_status_unpack(
    struct demo_status_t *dst_p,
    const uint8_t *src_p,
    size_t size)
{
    uint16_t controller_temp;
    uint16_t motor_temp;

    if (size < 8u) {
        return (-EINVAL);
    }

    dst_p->seat = unpack_right_shift_u8(src_p[0], 0u, 0x01u);
    dst_p->brake = unpack_right_shift_u8(src_p[0], 1u, 0x02u);
    dst_p->neutral = unpack_right_shift_u8(src_p[0], 2u, 0x04u);
    dst_p->warning = unpack_right_shift_u8(src_p[0], 3u, 0x08u);
    motor_temp = unpack_right_shift_u16(src_p[1], 0u, 0xffu);
    motor_temp |= unpack_left_shift_u16(src_p[2], 8u, 0xffu);
    dst_p->motor_temp = (int16_t)motor_temp;
    controller_temp = unpack_right_shift_u16(src_p[3], 0u, 0xffu);
    controller_temp |= unpack_left_shift_u16(src_p[4], 8u, 0xffu);
    dst_p->controller_temp = (int16_t)controller_temp;

    return (0);
}

int demo_status_init(struct demo_status_t *msg_p)
{
    if (msg_p == NULL) return -1;

    memset(msg_p, 0, sizeof(struct demo_status_t));

    return 0;
}

uint8_t demo_status_seat_encode(float value)
{
    return (uint8_t)(value);
}

float demo_status_seat_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_status_seat_is_in_range(uint8_t value)
{
    return (value <= 1u);
}

uint8_t demo_status_brake_encode(float value)
{
    return (uint8_t)(value);
}

float demo_status_brake_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_status_brake_is_in_range(uint8_t value)
{
    return (value <= 1u);
}

uint8_t demo_status_neutral_encode(float value)
{
    return (uint8_t)(value);
}

float demo_status_neutral_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_status_neutral_is_in_range(uint8_t value)
{
    return (value <= 1u);
}

uint8_t demo_status_warning_encode(float value)
{
    return (uint8_t)(value);
}

float demo_status_warning_decode(uint8_t value)
{
    return ((float)value);
}

bool demo_status_warning_is_in_range(uint8_t value)
{
    return (value <= 1u);
}

int16_t demo_status_motor_temp_encode(float value)
{
    return (int16_t)(value);
}

float demo_status_motor_temp_decode(int16_t value)
{
    return ((float)value);
}

bool demo_status_motor_temp_is_in_range(int16_t value)
{
    return ((value >= -40) && (value <= 150));
}

int16_t demo_status_controller_temp_encode(float value)
{
    return (int16_t)(value);
}

float demo_status_controller_temp_decode(int16_t value)
{
    return ((float)value);
}

bool demo_status_controller_temp_is_in_range(int16_t value)
{
    return ((value >= -40) && (value <= 150));
}
