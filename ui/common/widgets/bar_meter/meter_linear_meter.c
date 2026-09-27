#include "ui/common/i18n/meter_i18n.h"
#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <string.h>
struct meter_linear_meter
{
    meter_language_t language;
    meter_widget_style_t style;
    lv_obj_t *root, *bar, *label;
    float min, max, value;
    const char *unit;
    meter_value_state_t state;
    char text[48];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
static void apply_style(meter_linear_meter_t *b)
{
    lv_obj_set_style_bg_color(b->bar, b->style.track, 0);
    lv_obj_set_style_text_color(b->label, b->style.text, 0);
    lv_obj_set_style_text_font(b->label, b->style.label_font, 0);
}
static void redraw(meter_linear_meter_t *b)
{
    float value = 0;
    meter_gauge_map(b->value, b->min, b->max, 0, 1000, &value);
    lv_bar_set_value(b->bar,
                     (b->state == METER_VALUE_UNKNOWN || b->state == METER_VALUE_ERROR) ? 0 : (int)value,
                     LV_ANIM_OFF);
    lv_obj_set_style_bg_color(b->bar, b->state == METER_VALUE_VALID ? b->style.primary : b->style.muted,
                              LV_PART_INDICATOR);
    meter_i18n_format_value(b->text, sizeof(b->text), b->value, b->state, b->unit, 2, b->language);
    lv_label_set_text_static(b->label, b->text);
}
meter_linear_meter_t *meter_linear_meter_create(lv_obj_t *p, int x, int y, int w, int h, float min, float max,
                                                const char *unit)
{
    if (min >= max)
        return NULL;
    meter_linear_meter_t *b = lv_malloc(sizeof(*b));
    if (!b)
        return NULL;
    memset(b, 0, sizeof(*b));
    b->style = *meter_widget_style_default();
    b->min = min;
    b->max = max;
    b->unit = unit;
    b->root = lv_obj_create(p);
    lv_obj_remove_style_all(b->root);
    lv_obj_set_pos(b->root, x, y);
    lv_obj_set_size(b->root, w, h);
    lv_obj_set_scrollable(b->root, false);
    lv_obj_add_event_cb(b->root, dispose, LV_EVENT_DELETE, b);
    b->bar = lv_bar_create(b->root);
    lv_obj_set_size(b->bar, 16, h - 8);
    lv_bar_set_range(b->bar, 0, 1000);
    b->label = meter_text(b->root, 29, h / 2 - 10, "", b->style.label_font, 0);
    apply_style(b);
    redraw(b);
    return b;
}
void meter_linear_meter_set_value(meter_linear_meter_t *b, float v)
{
    if (!isfinite(v))
        b->state = METER_VALUE_ERROR;
    else
        b->value = fmaxf(b->min, fminf(b->max, v));
    redraw(b);
}
void meter_linear_meter_set_state(meter_linear_meter_t *b, meter_value_state_t state)
{
    b->state = state;
    redraw(b);
}

void meter_linear_meter_set_language(meter_linear_meter_t *b, meter_language_t language)
{
    b->language = language;
    meter_i18n_apply_font(b->label, language);
    redraw(b);
}
void meter_linear_meter_set_style(meter_linear_meter_t *b, const meter_widget_style_t *style)
{
    if (style) b->style = *style;
    apply_style(b);
    redraw(b);
}
