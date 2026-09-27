#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/common/formatter/meter_format.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static const char *const languages[] = {METER_LANGUAGE_CODE_EN, METER_LANGUAGE_CODE_ZH, NULL};
static const char *const tags[] = {
    "VALIDITY_LIVE",
    "VALIDITY_STALE",
    "VALIDITY_UNKNOWN",
    "VALIDITY_ERROR",
    "STATE_ON",
    "STATE_OFF",
    NULL
};
static const char *const translations[] = {
    "LIVE", "实时",
    "STALE", "过期",
    "NO DATA", "无数据",
    "SENSOR ERROR", "传感器错误",
    "ON", "开",
    "OFF", "关",
};
_Static_assert(sizeof(tags) / sizeof(tags[0]) == METER_TXT_COUNT + 1, "generic tag count");
_Static_assert(sizeof(translations) / sizeof(translations[0]) == METER_TXT_COUNT * 2,
               "generic translation count");
static meter_font_resolver_t font_resolver;
bool meter_i18n_init(void)
{
    if (!lv_translation_add_static(languages, tags, translations))
        return false;
    lv_translation_set_language(METER_LANGUAGE_CODE_EN);
    return true;
}
const char *meter_i18n_language_code(meter_language_t language)
{
    return language == METER_LANGUAGE_ZH ? METER_LANGUAGE_CODE_ZH : METER_LANGUAGE_CODE_EN;
}
meter_language_t meter_i18n_selected_language(void)
{
    return lv_strcmp(lv_translation_get_language(), METER_LANGUAGE_CODE_ZH) == 0 ? METER_LANGUAGE_ZH
                                                                                 : METER_LANGUAGE_EN;
}
const char *meter_i18n_tag(meter_text_id_t id)
{
    return (unsigned)id < METER_TXT_COUNT ? tags[id] : "";
}
const char *meter_i18n_text(meter_text_id_t id)
{
    return meter_i18n_tr(meter_i18n_tag(id));
}
const char *meter_i18n_tr(const char *tag)
{
    return tag && tag[0] ? lv_tr(tag) : "";
}
const char *meter_i18n_state(meter_value_state_t state)
{
    return meter_i18n_text(state == METER_VALUE_UNKNOWN ? METER_TXT_VALIDITY_UNKNOWN
                                : state == METER_VALUE_STALE ? METER_TXT_VALIDITY_STALE
                                : state == METER_VALUE_ERROR ? METER_TXT_VALIDITY_ERROR
                                                             : METER_TXT_VALIDITY_LIVE);
}
void meter_font_provider_set(meter_font_resolver_t resolver)
{
    font_resolver = resolver;
}
meter_font_role_t meter_font_role_of(const lv_font_t *font)
{
    return font && font->line_height >= 24 ? METER_FONT_VALUE : METER_FONT_LABEL;
}
const lv_font_t *meter_font_get(meter_language_t language, meter_font_role_t role)
{
    const lv_font_t *font = font_resolver ? font_resolver(language, role) : NULL;
    if (font)
        return font;
    return role == METER_FONT_VALUE ? &lv_font_montserrat_24 : &lv_font_montserrat_14;
}
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language, meter_font_role_t role)
{
    if (label)
        lv_obj_set_style_text_font(label, meter_font_get(language, role), 0);
}
static void adopt_language_font(lv_obj_t *label, const lv_font_t *base)
{
    meter_i18n_apply_font(label, meter_i18n_selected_language(), meter_font_role_of(base));
}
static void language_changed(lv_event_t *event)
{
    /* User data keeps the face the label was designed with, which carries its size class. */
    adopt_language_font(lv_event_get_target_obj(event), lv_event_get_user_data(event));
}
void meter_i18n_bind_label(lv_obj_t *label, const char *tag)
{
    const lv_font_t *original = lv_obj_get_style_text_font(label, 0);
    lv_obj_add_event_cb(label, language_changed, LV_EVENT_TRANSLATION_LANGUAGE_CHANGED, (void *)original);
    lv_label_set_translation_tag(label, tag);
    /* English renders in the face the product picked, so only a substituted language resizes. */
    if (meter_i18n_selected_language() != METER_LANGUAGE_EN)
        adopt_language_font(label, original);
}
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language)
{
    if (language != METER_LANGUAGE_ZH)
    {
        meter_format_value(out, size, value, state, unit, decimals);
        return;
    }
    if (state == METER_VALUE_ERROR || !isfinite(value))
        snprintf(out, size, "错误 %s", unit);
    else
    {
        meter_format_value(out, size, value, state == METER_VALUE_STALE ? METER_VALUE_VALID : state, unit,
                           decimals);
        if (state == METER_VALUE_STALE && size)
        {
            size_t used = strlen(out);
            snprintf(out + used, size - used, " / %s", meter_i18n_text(METER_TXT_VALIDITY_STALE));
        }
    }
}
