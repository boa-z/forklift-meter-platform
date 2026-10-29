#include "contracts/hmi_product.h"
#include "catalog/catalog.h"
#include "ui/ui.h"
#include "i18n/generated/product_i18n.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h> /* [can] */
bool product_locale_init(void)
{
    return hmi_i18n_register_languages(product_i18n_languages, PRODUCT_I18N_LANGUAGE_COUNT) &&
        hmi_i18n_register_pack(product_i18n_pack);
}
static void *create(void *parent, const hmi_ui_actions_t *actions)
{
    (void)actions;
    /* 固件在调用这里之前已经执行过 product_locale_init()；主机模拟器在这里补一次，重复注册无害。 */
    if (!product_locale_init())
        return NULL;
    lv_obj_t *label = lv_label_create(parent);
    hmi_i18n_bind_font(label, HMI_FONT_LABEL);
    lv_obj_center(label);
    return label;
}
static void present(void *ui, const hmi_snapshot_t *s, uint32_t elapsed)
{
    (void)elapsed;
    char number[32], text[192];
    const hmi_value_t uptime = hmi_snapshot_read(s, PRODUCT_UPTIME);
    snprintf(number, sizeof(number), "%u", (unsigned)uptime.value);
    const hmi_i18n_arg_t args[] = {hmi_i18n_arg_str(number)};
    hmi_i18n_format(text, sizeof(text), hmi_i18n_tr("ui.uptime"), args, 1);
    const hmi_value_t v = hmi_snapshot_read(s, PRODUCT_SPEED); /* [can] */
    char speed[32], line[96]; /* [can] */
    snprintf(speed, sizeof(speed), "%.1f", (double)v.value); /* [can] */
    const hmi_i18n_arg_t can_args[] = {hmi_i18n_arg_str(speed), hmi_i18n_arg_u((unsigned)v.state)}; /* [can] */
    hmi_i18n_format(line, sizeof(line), hmi_i18n_tr("ui.speed"), can_args, 2); /* [can] */
    snprintf(text + strlen(text), sizeof(text) - strlen(text), "\n%s", line); /* [can] */
    lv_label_set_text(ui, text);
}
static void destroy(void *ui) { lv_obj_delete(ui); }
const hmi_ui_factory_t product_ui = {.create = create, .present = present, .destroy = destroy};
