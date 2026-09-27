/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#ifndef MIXED_H
#define MIXED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef EINVAL
#    define EINVAL 22
#endif


#define MIXED_MOTION_FRAME_ID (0x100u)
#define MIXED_ENERGY_FRAME_ID (0x101u)


#define MIXED_MOTION_LENGTH (8u)
#define MIXED_ENERGY_LENGTH (8u)


#define MIXED_MOTION_IS_EXTENDED (0)
#define MIXED_ENERGY_IS_EXTENDED (0)








#define MIXED_MOTION_NAME "motion"
#define MIXED_ENERGY_NAME "energy"


#define MIXED_MOTION_SPEED_NAME "speed"
#define MIXED_ENERGY_SOC_NAME "soc"


struct mixed_motion_t {

    uint16_t speed;
};


struct mixed_energy_t {

    uint8_t soc;
};


int mixed_motion_pack(
    uint8_t *dst_p,
    const struct mixed_motion_t *src_p,
    size_t size);


int mixed_motion_unpack(
    struct mixed_motion_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int mixed_motion_init(struct mixed_motion_t *msg_p);


uint16_t mixed_motion_speed_encode(float value);


float mixed_motion_speed_decode(uint16_t value);


bool mixed_motion_speed_is_in_range(uint16_t value);


int mixed_energy_pack(
    uint8_t *dst_p,
    const struct mixed_energy_t *src_p,
    size_t size);


int mixed_energy_unpack(
    struct mixed_energy_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int mixed_energy_init(struct mixed_energy_t *msg_p);


uint8_t mixed_energy_soc_encode(float value);


float mixed_energy_soc_decode(uint8_t value);


bool mixed_energy_soc_is_in_range(uint8_t value);


#ifdef __cplusplus
}
#endif

#endif
