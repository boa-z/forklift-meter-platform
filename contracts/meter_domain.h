#ifndef METER_DOMAIN_H
#define METER_DOMAIN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum
{
    METER_VALUE_UNKNOWN,
    METER_VALUE_VALID,
    METER_VALUE_STALE,
    METER_VALUE_ERROR
} meter_value_state_t;
typedef enum
{
    METER_LANGUAGE_EN = 0,
    METER_LANGUAGE_ZH = 1
} meter_language_t;
typedef enum
{
    METER_SPEED,
    METER_SOC,
    METER_HEIGHT,
    METER_LOAD,
    METER_STEERING,
    METER_WORK_HOURS,
    METER_BATTERY_VOLTAGE,
    METER_MOTOR_TEMP,
    METER_CONTROLLER_TEMP,
    METER_SEAT,
    METER_BRAKE,
    METER_NEUTRAL,
    METER_CHARGING,
    METER_WARNING,
    METER_SIGNAL_COUNT
} meter_signal_id_t;
typedef struct
{
    float value;
    uint32_t timestamp_ms;
    meter_value_state_t state;
} meter_value_t;
typedef struct
{
    meter_signal_id_t signal;
    meter_value_t value;
} meter_update_t;
typedef struct
{
    uint16_t id;
    const char *key;
    const char *unit;
    float min, max, initial;
} meter_parameter_def_t;
typedef struct
{
    const char *key;
    const char *unit;
    meter_signal_id_t signal;
} meter_monitor_def_t;
typedef struct
{
    uint16_t id;
    const char *key;
    const char *description;
} meter_fault_def_t;
typedef struct
{
    const meter_parameter_def_t *parameters;
    size_t parameter_count;
    const meter_monitor_def_t *monitors;
    size_t monitor_count;
    const meter_fault_def_t *faults;
    size_t fault_count;
} meter_catalog_t;
#define METER_PARAMETER_CAPACITY 16u
#define METER_MONITOR_CAPACITY 32u
#define METER_FAULT_CAPACITY 16u
typedef struct
{
    meter_value_t signals[METER_SIGNAL_COUNT];
    float parameters[METER_PARAMETER_CAPACITY];
    uint32_t active_faults;
    uint32_t generation;
    bool connected;
    bool imperial;
    meter_language_t language;
    uint8_t brightness;
} meter_snapshot_t;
typedef enum
{
    METER_ACTION_UNITS,
    METER_ACTION_BRIGHTNESS,
    METER_ACTION_PARAMETER,
    METER_ACTION_LANGUAGE
} meter_action_kind_t;
typedef struct
{
    meter_action_kind_t kind;
    uint16_t id;
    float value;
} meter_action_t;
typedef bool (*meter_action_send_t)(void *context, const meter_action_t *action);
typedef struct
{
    meter_action_send_t send;
    void *context;
} meter_ui_actions_t;
typedef struct
{
    bool (*load)(void *context, uint8_t *data, size_t capacity, size_t *size);
    bool (*save)(void *context, const uint8_t *data, size_t size);
    void *context;
} meter_persistence_port_t;
#endif
