/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#ifndef PRODUCT_H
#define PRODUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef EINVAL
#    define EINVAL 22
#endif


#define PRODUCT_DRIVETRAIN_FRAME_ID (0x18ff5201u)
#define PRODUCT_POWER_FRAME_ID (0x18ff5202u)


#define PRODUCT_DRIVETRAIN_LENGTH (8u)
#define PRODUCT_POWER_LENGTH (8u)


#define PRODUCT_DRIVETRAIN_IS_EXTENDED (1)
#define PRODUCT_POWER_IS_EXTENDED (1)








#define PRODUCT_DRIVETRAIN_NAME "drivetrain"
#define PRODUCT_POWER_NAME "power"


#define PRODUCT_DRIVETRAIN_VELOCITY_NAME "velocity"
#define PRODUCT_DRIVETRAIN_TORQUE_NAME "torque"
#define PRODUCT_POWER_REMAINING_NAME "remaining"
#define PRODUCT_POWER_AMBIENT_NAME "ambient"


struct product_drivetrain_t {
    
    uint16_t velocity;

    
    int16_t torque;
};


struct product_power_t {
    
    uint8_t remaining;

    
    uint16_t ambient;
};


int product_drivetrain_pack(
    uint8_t *dst_p,
    const struct product_drivetrain_t *src_p,
    size_t size);


int product_drivetrain_unpack(
    struct product_drivetrain_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int product_drivetrain_init(struct product_drivetrain_t *msg_p);


uint16_t product_drivetrain_velocity_encode(float value);


float product_drivetrain_velocity_decode(uint16_t value);


bool product_drivetrain_velocity_is_in_range(uint16_t value);


int16_t product_drivetrain_torque_encode(float value);


float product_drivetrain_torque_decode(int16_t value);


bool product_drivetrain_torque_is_in_range(int16_t value);


int product_power_pack(
    uint8_t *dst_p,
    const struct product_power_t *src_p,
    size_t size);


int product_power_unpack(
    struct product_power_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int product_power_init(struct product_power_t *msg_p);


uint8_t product_power_remaining_encode(float value);


float product_power_remaining_decode(uint8_t value);


bool product_power_remaining_is_in_range(uint8_t value);


uint16_t product_power_ambient_encode(float value);


float product_power_ambient_decode(uint16_t value);


bool product_power_ambient_is_in_range(uint16_t value);


#ifdef __cplusplus
}
#endif

#endif
