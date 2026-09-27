#include "ui/common/i18n/meter_i18n.h"
#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <string.h>
struct meter_ring
{
    meter_language_t language;
    meter_widget_style_t style;
    lv_obj_t *arc, *label, *validity;
    float min, max, value, warning, low;
    bool thresholds;
    const char *unit;
    meter_value_state_t state;
    char text[48];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
static void apply_style(meter_ring_t *r)
{
    lv_obj_set_style_arc_color(r->arc, r->style.track, LV_PART_MAIN);
    lv_obj_set_style_text_color(r->label, r->style.text, 0);
    lv_obj_set_style_text_color(r->validity, r->style.warning, 0);
}
static void redraw(meter_ring_t *r)
{
    float mapped = 0;
    meter_gauge_map(r->value, r->min, r->max, 0, 1000, &mapped);
    bool missing = r->state == METER_VALUE_UNKNOWN || r->state == METER_VALUE_ERROR;
    lv_arc_set_value(r->arc, missing ? 0 : (int)mapped);
    unsigned band = r->thresholds ? meter_threshold_band(r->value, r->warning, r->low) : 2;
    lv_color_t colors[] = {r->style.error, r->style.warning, r->style.primary};
    lv_obj_set_style_arc_color(r->arc, r->state == METER_VALUE_STALE ? r->style.muted : colors[band],
                               LV_PART_INDICATOR);
    meter_i18n_format_value(r->text, sizeof(r->text), r->value, missing ? r->state : METER_VALUE_VALID, r->unit,
                       0, r->language);
    lv_label_set_text_static(r->label, r->text);
    lv_obj_center(r->label);
    lv_label_set_text_static(r->validity, r->state == METER_VALUE_VALID ? "" : meter_i18n_state(r->state));
    lv_obj_align(r->validity, LV_ALIGN_CENTER, 0, 24);
}
meter_ring_t *meter_arc_bar_create(lv_obj_t *parent, int x, int y, int diameter, float start, float end,
                                   int stroke, const char *unit)
{
    if (!isfinite(start) || !isfinite(end) || diameter <= 0 || stroke < 1 || stroke * 2 > diameter || start < 0 ||
        start > 360 || end < 0 || end > 360 || start >= end)
        return NULL;
    meter_ring_t *r = lv_malloc(sizeof(*r));
    if (!r)
        return NULL;
    memset(r, 0, sizeof(*r));
    r->max = 100;
    r->unit = unit;
    r->style = *meter_widget_style_default();
    if (diameter < 150)
        r->style.value_font = &lv_font_montserrat_16;
    r->arc = lv_arc_create(parent);
    lv_obj_set_pos(r->arc, x, y);
    lv_obj_set_size(r->arc, diameter, diameter);
    lv_arc_set_bg_angles(r->arc, start, end);
    lv_arc_set_range(r->arc, 0, 1000);
    lv_obj_remove_style(r->arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(r->arc, false);
    lv_obj_set_style_arc_width(r->arc, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_width(r->arc, stroke, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(r->arc, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_event_cb(r->arc, dispose, LV_EVENT_DELETE, r);
    r->label = meter_text(r->arc, 0, 0, "", r->style.value_font, 0);
    r->validity = meter_text(r->arc, 0, 0, "", r->style.label_font, 0);
    apply_style(r);
    redraw(r);
    return r;
}
meter_ring_t *meter_ring_create(lv_obj_t *p, int x, int y, int d, const char *unit)
{
    meter_ring_t *r = meter_arc_bar_create(p, x, y, d, 0, 360, 10, unit);
    if (r)
        lv_arc_set_rotation(r->arc, 270);
    return r;
}
bool meter_ring_set_range(meter_ring_t *r, float min, float max)
{
    if (!isfinite(min) || !isfinite(max) || min >= max)
        return false;
    r->min = min;
    r->max = max;
    redraw(r);
    return true;
}
void meter_ring_set_thresholds(meter_ring_t *r, float warning, float low)
{
    if (!isfinite(warning) || !isfinite(low) || warning > low)
        return;
    r->warning = warning;
    r->low = low;
    r->thresholds = true;
    redraw(r);
}
void meter_ring_set_value(meter_ring_t *r, float value)
{
    if (!isfinite(value))
        r->state = METER_VALUE_ERROR;
    else
        r->value = fmaxf(r->min, fminf(r->max, value));
    redraw(r);
}
void meter_ring_set_state(meter_ring_t *r, meter_value_state_t state)
{
    r->state = state;
    redraw(r);
}
void meter_ring_set_unit(meter_ring_t *r, const char *unit)
{
    r->unit = unit;
    redraw(r);
}
void meter_ring_set_style(meter_ring_t *r, const meter_widget_style_t *style)
{
    if (style) r->style = *style;
    apply_style(r);
    meter_i18n_apply_font(r->label, r->language);
    redraw(r);
}

void meter_ring_set_language(meter_ring_t *r, meter_language_t language)
{
    r->language = language;
    meter_i18n_apply_font(r->label, language);
    meter_i18n_apply_font(r->validity, language);
    redraw(r);
}
