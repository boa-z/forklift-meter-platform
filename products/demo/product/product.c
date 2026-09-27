#include "product/product.h"
#include "generated/demo_catalog.h"
#ifdef METER_HEADLESS
#define PRODUCT_UI NULL
#else
#define PRODUCT_UI &meter_demo_ui
#endif
static const meter_product_t product = {"reference-demo",   &meter_demo_capabilities, &meter_demo_protocols,
                                        &meter_demo_routes, PRODUCT_UI,              &meter_demo_resources,
                                        &meter_demo_locale, &meter_demo_auth,         &meter_demo_catalog,
                                        meter_demo_evaluate};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
