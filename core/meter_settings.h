#ifndef METER_SETTINGS_H
#define METER_SETTINGS_H
#include "core/meter_core.h"
#define METER_SETTINGS_SIZE (12u + METER_PARAMETER_CAPACITY * 4u)
bool meter_settings_encode(const meter_core_t *core, uint8_t out[METER_SETTINGS_SIZE]);
bool meter_settings_decode(meter_core_t *core, const uint8_t *data, size_t size);
#endif
