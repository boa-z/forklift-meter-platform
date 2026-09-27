#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <string.h>
struct meter_ring
{
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
static void redraw(meter_ring_t *r)
{
    float mapped = 0;
    meter_gauge_map(r->value, r->min, r->max, 0, 1000, &mapped);
    bool missing = r->state == METER_VALUE_UNKNOWN || r->state == METER_VALUE_ERROR;
    lv_arc_set_value(r->arc, missing ? 0 : (int)mapped);
    unsigned band = r->thresholds ? meter_threshold_band(r->value, r->warning, r->low) : 2;
    uint32_t colors[] = {0xff856d, 0xf3ba65, 0x5de5ca};
    lv_obj_set_style_arc_color(r->arc, lv_color_hex(r->state == METER_VALUE_STALE ? 0x8299a9 : colors[band]),
                               LV_PART_INDICATOR);
    meter_format_value(r->text, sizeof(r->text), r->value, missing ? r->state : METER_VALUE_VALID, r->unit,
                       0);
    lv_label_set_text_static(r->label, r->text);
    lv_obj_center(r->label);
    lv_label_set_text_static(r->validity, r->state == METER_VALUE_STALE     ? "STALE"
                                          : r->state == METER_VALUE_UNKNOWN ? "NO DATA"
                                          : r->state == METER_VALUE_ERROR   ? "ERROR"
                                                                            : "");
    lv_obj_align(r->validity, LV_ALIGN_CENTER, 0, 24);
}
meter_ring_t *meter_arc_bar_create(lv_obj_t *parent, int x, int y, int diameter, float start, float end,
                                   int stroke, const char *unit)
{
    meter_ring_t *r = lv_malloc(sizeof(*r));
    if (!r)
        return NULL;
    memset(r, 0, sizeof(*r));
    r->max = 100;
    r->unit = unit;
    r->arc = lv_arc_create(parent);
    lv_obj_set_pos(r->arc, x, y);
    lv_obj_set_size(r->arc, diameter, diameter);
    lv_arc_set_bg_angles(r->arc, start, end);
    lv_arc_set_range(r->arc, 0, 1000);
    lv_obj_remove_style(r->arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(r->arc, false);
    lv_obj_set_style_arc_width(r->arc, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_width(r->arc, stroke, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(r->arc, lv_color_hex(0x263e4e), LV_PART_MAIN);
    lv_obj_add_event_cb(r->arc, dispose, LV_EVENT_DELETE, r);
    r->label = meter_text(r->arc, 0, 0, "", diameter >= 150 ? &lv_font_montserrat_24 : &lv_font_montserrat_16,
                          0xedf5f8);
    r->validity = meter_text(r->arc, 0, 0, "", &lv_font_montserrat_12, 0xf3ba65);
    redraw(r);
    return r;
}
meter_ring_t *meter_ring_create(lv_obj_t *p, int x, int y, int d, const char *unit)
{
    return meter_arc_bar_create(p, x, y, d, 270, 630, 10, unit);
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
