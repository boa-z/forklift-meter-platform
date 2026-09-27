/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */
#ifndef DEMO_H
#define DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef EINVAL
#    define EINVAL 22
#endif


#define DEMO_MOTION_FRAME_ID (0x100u)
#define DEMO_ENERGY_FRAME_ID (0x101u)
#define DEMO_LIFT_FRAME_ID (0x102u)
#define DEMO_LOAD_FRAME_ID (0x103u)
#define DEMO_STATUS_FRAME_ID (0x104u)


#define DEMO_MOTION_LENGTH (8u)
#define DEMO_ENERGY_LENGTH (8u)
#define DEMO_LIFT_LENGTH (8u)
#define DEMO_LOAD_LENGTH (8u)
#define DEMO_STATUS_LENGTH (8u)


#define DEMO_MOTION_IS_EXTENDED (0)
#define DEMO_ENERGY_IS_EXTENDED (0)
#define DEMO_LIFT_IS_EXTENDED (0)
#define DEMO_LOAD_IS_EXTENDED (0)
#define DEMO_STATUS_IS_EXTENDED (0)








#define DEMO_MOTION_NAME "motion"
#define DEMO_ENERGY_NAME "energy"
#define DEMO_LIFT_NAME "lift"
#define DEMO_LOAD_NAME "load"
#define DEMO_STATUS_NAME "status"


#define DEMO_MOTION_SPEED_NAME "speed"
#define DEMO_MOTION_STEERING_NAME "steering"
#define DEMO_MOTION_HOURS_NAME "hours"
#define DEMO_ENERGY_SOC_NAME "soc"
#define DEMO_ENERGY_VOLTAGE_NAME "voltage"
#define DEMO_ENERGY_CHARGING_NAME "charging"
#define DEMO_LIFT_HEIGHT_NAME "height"
#define DEMO_LOAD_WEIGHT_NAME "weight"
#define DEMO_STATUS_SEAT_NAME "seat"
#define DEMO_STATUS_BRAKE_NAME "brake"
#define DEMO_STATUS_NEUTRAL_NAME "neutral"
#define DEMO_STATUS_WARNING_NAME "warning"
#define DEMO_STATUS_MOTOR_TEMP_NAME "motor_temp"
#define DEMO_STATUS_CONTROLLER_TEMP_NAME "controller_temp"


struct demo_motion_t {

    uint16_t speed;


    int16_t steering;


    uint16_t hours;
};


struct demo_energy_t {

    uint8_t soc;


    uint16_t voltage;


    uint8_t charging;
};


struct demo_lift_t {

    uint16_t height;
};


struct demo_load_t {

    uint16_t weight;
};


struct demo_status_t {

    uint8_t seat;


    uint8_t brake;


    uint8_t neutral;


    uint8_t warning;


    int16_t motor_temp;


    int16_t controller_temp;
};


int demo_motion_pack(
    uint8_t *dst_p,
    const struct demo_motion_t *src_p,
    size_t size);


int demo_motion_unpack(
    struct demo_motion_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int demo_motion_init(struct demo_motion_t *msg_p);


uint16_t demo_motion_speed_encode(float value);


float demo_motion_speed_decode(uint16_t value);


bool demo_motion_speed_is_in_range(uint16_t value);


int16_t demo_motion_steering_encode(float value);


float demo_motion_steering_decode(int16_t value);


bool demo_motion_steering_is_in_range(int16_t value);


uint16_t demo_motion_hours_encode(float value);


float demo_motion_hours_decode(uint16_t value);


bool demo_motion_hours_is_in_range(uint16_t value);


int demo_energy_pack(
    uint8_t *dst_p,
    const struct demo_energy_t *src_p,
    size_t size);


int demo_energy_unpack(
    struct demo_energy_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int demo_energy_init(struct demo_energy_t *msg_p);


uint8_t demo_energy_soc_encode(float value);


float demo_energy_soc_decode(uint8_t value);


bool demo_energy_soc_is_in_range(uint8_t value);


uint16_t demo_energy_voltage_encode(float value);


float demo_energy_voltage_decode(uint16_t value);


bool demo_energy_voltage_is_in_range(uint16_t value);


uint8_t demo_energy_charging_encode(float value);


float demo_energy_charging_decode(uint8_t value);


bool demo_energy_charging_is_in_range(uint8_t value);


int demo_lift_pack(
    uint8_t *dst_p,
    const struct demo_lift_t *src_p,
    size_t size);


int demo_lift_unpack(
    struct demo_lift_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int demo_lift_init(struct demo_lift_t *msg_p);


uint16_t demo_lift_height_encode(float value);


float demo_lift_height_decode(uint16_t value);


bool demo_lift_height_is_in_range(uint16_t value);


int demo_load_pack(
    uint8_t *dst_p,
    const struct demo_load_t *src_p,
    size_t size);


int demo_load_unpack(
    struct demo_load_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int demo_load_init(struct demo_load_t *msg_p);


uint16_t demo_load_weight_encode(float value);


float demo_load_weight_decode(uint16_t value);


bool demo_load_weight_is_in_range(uint16_t value);


int demo_status_pack(
    uint8_t *dst_p,
    const struct demo_status_t *src_p,
    size_t size);


int demo_status_unpack(
    struct demo_status_t *dst_p,
    const uint8_t *src_p,
    size_t size);


int demo_status_init(struct demo_status_t *msg_p);


uint8_t demo_status_seat_encode(float value);


float demo_status_seat_decode(uint8_t value);


bool demo_status_seat_is_in_range(uint8_t value);


uint8_t demo_status_brake_encode(float value);


float demo_status_brake_decode(uint8_t value);


bool demo_status_brake_is_in_range(uint8_t value);


uint8_t demo_status_neutral_encode(float value);


float demo_status_neutral_decode(uint8_t value);


bool demo_status_neutral_is_in_range(uint8_t value);


uint8_t demo_status_warning_encode(float value);


float demo_status_warning_decode(uint8_t value);


bool demo_status_warning_is_in_range(uint8_t value);


int16_t demo_status_motor_temp_encode(float value);


float demo_status_motor_temp_decode(int16_t value);


bool demo_status_motor_temp_is_in_range(int16_t value);


int16_t demo_status_controller_temp_encode(float value);


float demo_status_controller_temp_decode(int16_t value);


bool demo_status_controller_temp_is_in_range(int16_t value);


#ifdef __cplusplus
}
#endif

#endif
