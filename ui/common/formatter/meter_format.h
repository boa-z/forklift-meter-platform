#ifndef METER_FORMAT_H
#define METER_FORMAT_H
#include "contracts/meter_domain.h"
bool meter_gauge_map(float value, float min, float max, float start, float end, float *angle);
void meter_format_value(char *out, size_t size, float value, meter_value_state_t state, const char *unit,
                        unsigned decimals);
unsigned meter_threshold_band(float value, float warning, float low);
#endif
