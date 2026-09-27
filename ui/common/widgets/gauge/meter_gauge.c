#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/common/widgets/needle/meter_needle.h"
#include <math.h>
#include <string.h>
struct meter_gauge
{
    lv_obj_t *root, *scale, *needle, *value, *state_label;
    lv_point_precise_t points[2];
    meter_gauge_config_t config;
    float current, from, target;
    uint32_t elapsed, duration;
    meter_value_state_t state;
    char text[48];
    char validity[16];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
static void redraw(meter_gauge_t *g)
{
    float angle;
    if (!meter_gauge_map(g->current, g->config.min, g->config.max, g->config.start_angle, g->config.end_angle,
                         &angle))
        return;
    meter_needle_points(g->points, angle, g->config.diameter, g->config.needle_length);
    lv_line_set_points(g->needle, g->points, 2);
    bool missing = g->state == METER_VALUE_UNKNOWN || g->state == METER_VALUE_ERROR;
    if (missing)
        lv_obj_set_hidden(g->needle, true);
    else
        lv_obj_set_hidden(g->needle, false);
    lv_obj_set_style_line_color(g->needle, lv_color_hex(g->state == METER_VALUE_STALE ? 0xf3ba65 : 0x5de5ca),
                                0);
    meter_format_value(g->text, sizeof(g->text), g->current, missing ? g->state : METER_VALUE_VALID,
                       g->config.unit, 1);
    lv_label_set_text_static(g->value, g->text);
    lv_obj_align(g->value, LV_ALIGN_BOTTOM_MID, 0, -26);
    strcpy(g->validity, g->state == METER_VALUE_STALE     ? "STALE"
                        : g->state == METER_VALUE_ERROR   ? "SENSOR ERROR"
                        : g->state == METER_VALUE_UNKNOWN ? "NO DATA"
                                                          : "LIVE");
    lv_label_set_text_static(g->state_label, g->validity);
    lv_obj_align(g->state_label, LV_ALIGN_BOTTOM_MID, 0, -7);
}
meter_gauge_t *meter_gauge_create(lv_obj_t *parent, int x, int y, const meter_gauge_config_t *c)
{
    if (!c || c->min >= c->max || c->ticks < 2 || !c->major_every || c->diameter < 50)
        return NULL;
    meter_gauge_t *g = lv_malloc(sizeof(*g));
    if (!g)
        return NULL;
    memset(g, 0, sizeof(*g));
    g->config = *c;
    g->current = c->min;
    g->root = lv_obj_create(parent);
    lv_obj_remove_style_all(g->root);
    lv_obj_set_size(g->root, c->diameter, c->diameter);
    lv_obj_set_pos(g->root, x, y);
    lv_obj_set_scrollable(g->root, false);
    lv_obj_add_event_cb(g->root, dispose, LV_EVENT_DELETE, g);
    g->scale = lv_scale_create(g->root);
    lv_obj_set_size(g->scale, c->diameter, c->diameter);
    lv_scale_set_mode(g->scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(g->scale, (int32_t)c->min, (int32_t)c->max);
    lv_scale_set_total_tick_count(g->scale, c->ticks);
    lv_scale_set_major_tick_every(g->scale, c->major_every);
    lv_scale_set_angle_range(g->scale, (uint32_t)(c->end_angle - c->start_angle));
    lv_scale_set_rotation(g->scale, (int)c->start_angle);
    lv_obj_set_style_arc_width(g->scale, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g->scale, lv_color_hex(0x334858), LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, lv_color_hex(0x567084), LV_PART_ITEMS);
    lv_obj_set_style_line_width(g->scale, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(g->scale, lv_color_hex(0xc9dbe5), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g->scale, 2, LV_PART_INDICATOR);
    lv_obj_set_style_length(g->scale, 7, LV_PART_ITEMS);
    lv_obj_set_style_length(g->scale, 13, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g->scale, lv_color_hex(0xabc0cd), LV_PART_INDICATOR);
    lv_obj_set_style_text_font(g->scale, &lv_font_montserrat_14, LV_PART_INDICATOR);
    g->needle = lv_line_create(g->root);
    lv_obj_set_style_line_width(g->needle, 4, 0);
    lv_obj_set_style_line_rounded(g->needle, true, 0);
    lv_obj_t *hub = lv_obj_create(g->root);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 12, 12);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(0xe4f2f5), 0);
    lv_obj_set_style_bg_opa(hub, 255, 0);
    g->value = meter_text(g->root, 0, 0, "",
                          c->diameter >= 220 ? &lv_font_montserrat_24 : &lv_font_montserrat_16, 0xf1f7fa);
    g->state_label = meter_text(g->root, 0, 0, "", &lv_font_montserrat_12, 0x7895a8);
    redraw(g);
    return g;
}
bool meter_gauge_set_range(meter_gauge_t *g, float min, float max)
{
    if (!isfinite(min) || !isfinite(max) || min >= max)
        return false;
    if (g->config.min == min && g->config.max == max)
        return true;
    g->config.min = min;
    g->config.max = max;
    lv_scale_set_range(g->scale, (int32_t)min, (int32_t)max);
    redraw(g);
    return true;
}
void meter_gauge_set_unit(meter_gauge_t *g, const char *unit)
{
    g->config.unit = unit;
}
void meter_gauge_set_value(meter_gauge_t *g, float value)
{
    if (!isfinite(value))
    {
        meter_gauge_set_state(g, METER_VALUE_ERROR);
        return;
    }
    g->current = g->target = fmaxf(g->config.min, fminf(g->config.max, value));
    g->duration = 0;
    redraw(g);
}
void meter_gauge_set_value_animated(meter_gauge_t *g, float value, uint32_t duration)
{
    if (!isfinite(value))
    {
        meter_gauge_set_state(g, METER_VALUE_ERROR);
        return;
    }
    value = fmaxf(g->config.min, fminf(g->config.max, value));
    if (value == g->target && g->duration)
        return;
    g->from = g->current;
    g->target = value;
    g->elapsed = 0;
    g->duration = duration;
    if (!duration)
        meter_gauge_set_value(g, value);
}
void meter_gauge_set_state(meter_gauge_t *g, meter_value_state_t state)
{
    g->state = state;
    redraw(g);
}
void meter_gauge_advance(meter_gauge_t *g, uint32_t dt)
{
    if (g->duration)
    {
        g->elapsed = dt >= g->duration - g->elapsed ? g->duration : g->elapsed + dt;
        float t = g->elapsed / (float)g->duration;
        g->current = g->from + (g->target - g->from) * t;
        if (g->elapsed == g->duration)
            g->duration = 0;
    }
    redraw(g);
}
