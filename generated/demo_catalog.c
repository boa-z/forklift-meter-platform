/* 本文件由 tools/generate_catalog.py 自动生成，请勿手工修改；数据源：schema/demo_catalog.json。 */
#include "generated/demo_catalog.h"
static const meter_signal_def_t signals[] = {
    {1, "speed", "km/h", 750},
    {2, "battery_charge", "%", 1500},
    {3, "lift_height", "m", 750},
    {4, "load_weight", "kg", 750},
    {5, "steering_angle", "deg", 750},
    {6, "work_hours", "h", 3000},
    {7, "battery_voltage", "V", 1500},
    {8, "motor_temperature", "C", 1500},
    {9, "controller_temperature", "C", 1500},
    {10, "seat_occupied", "", 1500},
    {11, "parking_brake", "", 1500},
    {12, "neutral", "", 1500},
    {13, "charging", "", 1500},
    {14, "generic_warning", "", 1500},
};
static const meter_parameter_def_t parameters[] = {
    {1, "max_speed", "km/h", 5.0f, 50.0f, 25.0f},
    {2, "acceleration_ramp", "s", 1.0f, 10.0f, 3.0f},
    {3, "lift_limit", "m", 1.0f, 6.0f, 4.5f},
    {4, "battery_warning_level", "%", 5.0f, 35.0f, 20.0f},
    {5, "load_advisory", "kg", 100.0f, 1500.0f, 1000.0f},
    {6, "motor_temp_advisory", "C", 40.0f, 110.0f, 85.0f},
    {7, "controller_temp_advisory", "C", 40.0f, 100.0f, 75.0f},
    {8, "idle_dim_delay", "s", 10.0f, 120.0f, 30.0f},
    {9, "demo_cycle", "s", 10.0f, 120.0f, 40.0f},
    {10, "steering_limit", "deg", 10.0f, 45.0f, 45.0f},
};
static const meter_monitor_def_t monitors[] = {
    {"Vehicle speed", "km/h", METER_SPEED},
    {"Battery charge", "%", METER_SOC},
    {"Lift height", "m", METER_HEIGHT},
    {"Load weight", "kg", METER_LOAD},
    {"Steering angle", "deg", METER_STEERING},
    {"Work hours", "h", METER_WORK_HOURS},
    {"Battery voltage", "V", METER_BATTERY_VOLTAGE},
    {"Motor temperature", "C", METER_MOTOR_TEMP},
    {"Controller temperature", "C", METER_CONTROLLER_TEMP},
    {"Seat occupied", "", METER_SEAT},
    {"Parking brake", "", METER_BRAKE},
    {"Neutral", "", METER_NEUTRAL},
    {"Charging", "", METER_CHARGING},
    {"Generic warning", "", METER_WARNING},
};
static const meter_fault_def_t faults[] = {
    {1, "low_charge", "Battery charge below demo threshold"},
    {2, "overtemperature", "Motor temperature advisory"},
    {3, "low_voltage", "Battery voltage advisory"},
    {4, "sensor_fault", "Synthetic sensor reports an error"},
    {5, "communication_fault", "Telemetry unavailable or stale"},
    {6, "load_advisory", "Load above demo advisory"},
    {7, "height_advisory", "Lift above demo advisory"},
    {8, "controller_hot", "Controller temperature advisory"},
    {9, "generic_warning", "Injected demonstration warning"},
    {10, "steering_advisory", "Steering angle advisory"},
};
const meter_catalog_t meter_demo_catalog = {
    signals, sizeof(signals)/sizeof(signals[0]),
    parameters, sizeof(parameters)/sizeof(parameters[0]),
    monitors, sizeof(monitors)/sizeof(monitors[0]),
    faults, sizeof(faults)/sizeof(faults[0]),
    NULL, NULL
};
