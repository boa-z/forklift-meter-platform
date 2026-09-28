#include "catalog/catalog.h"
static const meter_signal_def_t signals[] = {
    {.id = MIXED_CAN0_SPEED, .key = "mixed.can0.speed", .unit = "m/s", .stale_ms = 750},
    {.id = MIXED_CAN0_SOC, .key = "mixed.can0.soc", .unit = "%", .stale_ms = 2400},
    {.id = MIXED_PDO_SPEED, .key = "mixed.pdo.speed", .unit = "m/s", .stale_ms = 500},
    {.id = MIXED_PDO_TORQUE, .key = "mixed.pdo.torque", .unit = "Nm", .stale_ms = 500},
};
static const meter_parameter_def_t parameters[] = {
    {.id = MIXED_PARAM_MAX_SPEED, .key = "mixed.max_speed", .unit = "m/s", .min = 0.0f, .max = 50.0f, .initial = 25.0f},
    {.id = MIXED_PARAM_ACCEL, .key = "mixed.accel", .unit = "m/s^2", .min = 0.0f, .max = 10.0f, .initial = 2.0f},
};
const meter_catalog_t mixed_catalog = {.signals = signals,
                                       .signal_count = 4,
                                       .parameters = parameters,
                                       .parameter_count = 2};
