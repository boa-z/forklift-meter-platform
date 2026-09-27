#include "catalog/catalog.h"
static const meter_signal_def_t signals[] = {
    {MIXED_CAN0_SPEED, "mixed.can0.speed", "m/s", 750},
    {MIXED_CAN0_SOC, "mixed.can0.soc", "%", 2400},
    {MIXED_PDO_SPEED, "mixed.pdo.speed", "m/s", 500},
    {MIXED_PDO_TORQUE, "mixed.pdo.torque", "Nm", 500},
};
static const meter_parameter_def_t parameters[] = {
    {MIXED_PARAM_MAX_SPEED, "mixed.max_speed", "m/s", 0.0f, 50.0f, 25.0f},
    {MIXED_PARAM_ACCEL, "mixed.accel", "m/s^2", 0.0f, 10.0f, 2.0f},
};
const meter_catalog_t mixed_catalog = {.signals = signals,
                                       .signal_count = 4,
                                       .parameters = parameters,
                                       .parameter_count = 2};
