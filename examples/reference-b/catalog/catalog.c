#include "catalog/catalog.h"
static const meter_signal_def_t signals[] = {
    {.id = PRODUCT_SPEED, .key = "drive.velocity", .unit = "m/s", .stale_ms = 400}, {.id = REF_TORQUE, .key = "drive.torque", .unit = "Nm", .stale_ms = 600},
    {.id = REF_SOC, .key = "battery.remaining", .unit = "%", .stale_ms = 2400}, {.id = REF_AMBIENT, .key = "environment.temperature", .unit = "C", .stale_ms = 5000}};
const meter_catalog_t product_catalog={.signals=signals,.signal_count=4};
