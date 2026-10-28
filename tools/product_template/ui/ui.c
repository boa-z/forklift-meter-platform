#include "contracts/meter_product.h"
#include "catalog/catalog.h"
#include "ui/ui.h"
#include "i18n/generated/product_i18n.h"
#include <lvgl.h>
#include <stdio.h>
bool product_locale_init(void)
{
    return meter_i18n_register_languages(product_i18n_languages, PRODUCT_I18N_LANGUAGE_COUNT) &&
        meter_i18n_register_pack(product_i18n_pack);
}
static void *create(void *parent,const meter_ui_actions_t *actions)
{
    (void)actions;
    /* 主机与固件共用 UI 工厂；语言表与文案均由本 Product 提供。 */
    if (!product_locale_init())
        return NULL;
    lv_obj_t *label=lv_label_create(parent);
    meter_i18n_bind_font(label, METER_FONT_LABEL);
    lv_obj_center(label);
    return label;
}
static void present(void *ui,const meter_snapshot_t *s,uint32_t elapsed)
{
    (void)elapsed;
    meter_value_t v=meter_snapshot_read(s,PRODUCT_SPEED);
    char speed[32], text[192];
    snprintf(speed, sizeof(speed), "%.1f", (double)v.value);
    const meter_i18n_arg_t args[] = {meter_i18n_arg_str(speed), meter_i18n_arg_u((unsigned)v.state)};
    meter_i18n_format(text, sizeof(text), meter_i18n_tr("ui.speed"), args, 2);
    lv_label_set_text(ui, text);
}
static void destroy(void *ui) { lv_obj_delete(ui); }
const meter_ui_factory_t product_ui={.create=create,.present=present,.destroy=destroy};
