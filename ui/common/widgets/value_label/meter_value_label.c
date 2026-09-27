#include "ui/common/i18n/meter_i18n.h"
#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <string.h>
struct meter_value_label
{
    meter_language_t language;
    meter_widget_style_t style;
    lv_obj_t *label;
    const char *unit;
    float current, from, target;
    meter_value_state_t state;
    char text[48];
};
static void redraw(meter_value_label_t *v)
{
    meter_i18n_format_value(v->text, sizeof(v->text), v->current, v->state, v->unit, 0, v->language);
    lv_label_set_text_static(v->label, v->text);
}
static void animate(void *context, int32_t progress)
{
    meter_value_label_t *v = context;
    v->current = v->from + (v->target - v->from) * (progress / 1000.0f);
    redraw(v);
}
static void dispose(lv_event_t *e)
{
    void *v = lv_event_get_user_data(e);
    lv_anim_delete(v, animate);
    lv_free(v);
}
static void apply_style(meter_value_label_t *v)
{
    lv_obj_set_style_text_font(v->label, v->style.value_font, 0);
    lv_obj_set_style_text_color(v->label, v->style.text, 0);
}
meter_value_label_t *meter_value_label_create(lv_obj_t *p, int x, int y, const char *unit)
{
    meter_value_label_t *v = lv_malloc(sizeof(*v));
    if (!v)
        return NULL;
    memset(v, 0, sizeof(*v));
    v->style = *meter_widget_style_default();
    v->unit = unit;
    v->label = meter_text(p, x, y, "--", v->style.value_font, 0);
    apply_style(v);
    lv_obj_add_event_cb(v->label, dispose, LV_EVENT_DELETE, v);
    return v;
}
void meter_value_label_set(meter_value_label_t *v, float value, meter_value_state_t state)
{
    if (!isfinite(value))
        state = METER_VALUE_ERROR;
    else if (v->target != value)
    {
        lv_anim_delete(v, animate);
        v->from = v->current;
        v->target = value;
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, v);
        lv_anim_set_exec_cb(&animation, animate);
        lv_anim_set_values(&animation, 0, 1000);
        lv_anim_set_duration(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    v->state = state;
    redraw(v);
}

void meter_value_label_set_language(meter_value_label_t *v, meter_language_t language)
{
    v->language = language;
    meter_i18n_apply_font(v->label, language);
    redraw(v);
}
void meter_value_label_set_style(meter_value_label_t *v, const meter_widget_style_t *style)
{
    if (style) v->style = *style;
    apply_style(v);
    redraw(v);
}
