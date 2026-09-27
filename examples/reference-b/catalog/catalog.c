#include "catalog/catalog.h"
static const meter_signal_def_t signals[] = {
    {PRODUCT_SPEED,"drive.velocity","m/s",400}, {REF_TORQUE,"drive.torque","Nm",600},
    {REF_SOC,"battery.remaining","%",2400}, {REF_AMBIENT,"environment.temperature","C",5000}};
const meter_catalog_t product_catalog={.signals=signals,.signal_count=4};
