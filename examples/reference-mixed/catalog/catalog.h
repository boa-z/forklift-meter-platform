#ifndef REFERENCE_MIXED_CATALOG_H
#define REFERENCE_MIXED_CATALOG_H
#include "contracts/meter_domain.h"
enum
{
    MIXED_CAN0_SPEED = 2001,
    MIXED_CAN0_SOC = 2002,
    MIXED_PDO_SPEED = 2003,
    MIXED_PDO_TORQUE = 2004
};
enum
{
    MIXED_PARAM_MAX_SPEED = 3001,
    MIXED_PARAM_ACCEL = 3002
};
extern const meter_catalog_t mixed_catalog;
#endif
