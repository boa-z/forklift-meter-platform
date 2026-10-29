#ifndef PRODUCT_CATALOG_H
#define PRODUCT_CATALOG_H
#include "contracts/hmi_domain.h"
enum
{
    PRODUCT_UPTIME = 1,
    PRODUCT_SPEED = 501, /* [can] */
};
extern const hmi_catalog_t product_catalog;
#endif
