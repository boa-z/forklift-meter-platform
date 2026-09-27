#include <lvgl.h>
#include "ui/common/i18n/meter_i18n.h"
LV_FONT_DECLARE(meter_demo_cjk_14);
LV_FONT_DECLARE(meter_demo_cjk_20);
static void language_changed(lv_event_t *event)
{
    lv_obj_t *label = lv_event_get_target_obj(event);
    const lv_font_t *original = lv_event_get_user_data(event);
    lv_obj_set_style_text_font(label, original, 0);
    if (lv_strcmp(lv_translation_get_language(), "zh-CN") == 0)
        meter_i18n_apply_font(label, METER_LANGUAGE_ZH);
}
void meter_i18n_bind_label(lv_obj_t *label, meter_text_id_t id)
{
    const lv_font_t *original = lv_obj_get_style_text_font(label, 0);
    lv_obj_add_event_cb(label, language_changed, LV_EVENT_TRANSLATION_LANGUAGE_CHANGED, (void *)original);
    lv_label_set_translation_tag(label, meter_i18n_tag(id));
    if (lv_strcmp(lv_translation_get_language(), "zh-CN") == 0)
        meter_i18n_apply_font(label, METER_LANGUAGE_ZH);
}
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language)
{
    const lv_font_t *current = lv_obj_get_style_text_font(label, 0);
    bool large = current != &meter_demo_cjk_14 &&
                 (current == &meter_demo_cjk_20 || current->line_height >= 24);
    const lv_font_t *font = language == METER_LANGUAGE_ZH
        ? (large ? &meter_demo_cjk_20 : &meter_demo_cjk_14)
        : (large ? &lv_font_montserrat_24 : &lv_font_montserrat_14);
    lv_obj_set_style_text_font(label, font, 0);
}
