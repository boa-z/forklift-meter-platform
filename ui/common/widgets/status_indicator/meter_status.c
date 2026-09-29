#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/common/widgets/meter_widgets.h"
#include <string.h>
struct meter_status
{
    meter_language_t language;
    meter_widget_style_t style;
    lv_obj_t *root, *label, *icon;
    const char *name;
    bool active;
    meter_value_state_t state;
    char text[48];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
static void apply_style(meter_status_t *s)
{
    lv_obj_set_style_text_font(s->label, s->style.label_font, 0);
    lv_obj_set_style_text_color(s->label, s->style.muted, 0);
}
meter_status_t *meter_status_create(lv_obj_t *p, int x, int y, const char *name, const lv_image_dsc_t *icon)
{
    meter_status_t *s = lv_malloc(sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->style = *meter_widget_style_default();
    s->name = name;
    s->root = lv_obj_create(p);
    lv_obj_remove_style_all(s->root);
    lv_obj_set_pos(s->root, x, y);
    lv_obj_set_size(s->root, 140, 40);
    lv_obj_add_event_cb(s->root, dispose, LV_EVENT_DELETE, s);
    if (icon)
    {
        s->icon = lv_image_create(s->root);
        lv_image_set_src(s->icon, icon);
        lv_image_set_scale(s->icon, 176);
    }
    s->label = meter_text(s->root, 46, 10, name, s->style.label_font, 0);
    apply_style(s);
    return s;
}
void meter_status_set(meter_status_t *s, bool active, meter_value_state_t state)
{
    s->active = active;
    s->state = state;
    const char *tag = state == METER_VALUE_VALID
        ? meter_i18n_text(active ? METER_TXT_STATE_ON : METER_TXT_STATE_OFF)
        : meter_i18n_state(state);
    lv_snprintf(s->text, sizeof(s->text), "%s %s", s->name, tag);
    lv_label_set_text_static(s->label, s->text);
    lv_obj_set_style_text_color(
        s->label, state == METER_VALUE_VALID ? (active ? s->style.primary : s->style.muted) : s->style.warning, 0);
    if (s->icon)
    {
        lv_obj_set_style_image_opa(s->icon, active ? 255 : 100, 0);
        lv_obj_set_style_image_recolor(s->icon,
                                       state != METER_VALUE_VALID ? s->style.warning
                                       : active ? s->style.primary : s->style.error, 0);
        lv_obj_set_style_image_recolor_opa(s->icon, 255, 0);
    }
}
void meter_status_set_name(meter_status_t *s, const char *name)
{
    s->name = name;
}

void meter_status_set_language(meter_status_t *s, meter_language_t language)
{
    s->language = language;
    meter_i18n_apply_font(s->label, language, meter_font_role_of(s->style.label_font));
    meter_status_set(s, s->active, s->state);
}
void meter_status_set_style(meter_status_t *s, const meter_widget_style_t *style)
{
    if (style) s->style = *style;
    apply_style(s);
    meter_status_set(s, s->active, s->state);
}
