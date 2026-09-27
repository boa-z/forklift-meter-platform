#include "catalog/catalog.h"
static const meter_signal_def_t signals[] = {{PRODUCT_SPEED,"vehicle.speed","m/s",500}};
const meter_catalog_t product_catalog = {.signals=signals,.signal_count=1};
