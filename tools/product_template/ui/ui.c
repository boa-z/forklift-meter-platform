#include "contracts/meter_product.h"
#include "catalog/catalog.h"
#include <lvgl.h>
static void *create(void *parent,const meter_ui_actions_t *actions)
{
    (void)actions;
    lv_obj_t *label=lv_label_create(parent);
    lv_obj_center(label);
    return label;
}
static void present(void *ui,const meter_snapshot_t *s,uint32_t elapsed)
{
    (void)elapsed;
    meter_value_t v=meter_snapshot_read(s,PRODUCT_SPEED);
    lv_label_set_text_fmt(ui,"@PRODUCT_ID@: %.1f m/s (%u)",(double)v.value,(unsigned)v.state);
}
static void destroy(void *ui) { lv_obj_delete(ui); }
const meter_ui_factory_t product_ui={create,present,destroy};
