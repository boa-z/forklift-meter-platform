#include "product/product.h"
#include "ui/demo_ui.h"
const meter_ui_factory_t meter_demo_ui = {.create = demo_ui_create, .present = demo_ui_present,
                                         .destroy = demo_ui_destroy, .present_update = demo_ui_update};
