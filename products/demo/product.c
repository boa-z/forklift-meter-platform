#include "products/demo/product.h"
#include "generated/demo_catalog.h"
static const meter_product_t product = {"reference-demo",   &meter_demo_capabilities, &meter_demo_protocols,
                                        &meter_demo_routes, &meter_demo_ui,           &meter_demo_resources,
                                        &meter_demo_locale, &meter_demo_auth,         &meter_demo_catalog,
                                        meter_demo_evaluate};
const meter_product_t *meter_product_get(void)
{
    return &product;
}
