#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/common/widgets/needle/meter_needle.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
struct meter_gauge
{
    meter_language_t language;
    meter_widget_style_t style;
    lv_obj_t *root, *scale, *needle, *hub, *value, *state_label;
    lv_point_precise_t points[2];
    meter_gauge_config_t config;
    float current, from, target;
    meter_value_state_t state;
    char text[48];
    char validity[32];
};
static void animate(void *context, int32_t progress);
static void apply_style(meter_gauge_t *g)
{
    lv_obj_set_style_text_font(g->value, g->style.value_font, 0);
    lv_obj_set_style_text_font(g->state_label, g->style.label_font, 0);
    lv_obj_set_style_arc_color(g->scale, g->style.track, LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, g->style.muted, LV_PART_ITEMS);
    lv_obj_set_style_line_color(g->scale, g->style.text, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g->scale, g->style.muted, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g->hub, g->style.text, 0);
    lv_obj_set_style_text_color(g->value, g->style.text, 0);
    lv_obj_set_style_text_color(g->state_label, g->style.muted, 0);
}
static void dispose(lv_event_t *e)
{
    void *g = lv_event_get_user_data(e);
    lv_anim_delete(g, animate);
    lv_free(g);
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
    lv_obj_set_style_line_color(g->needle, g->state == METER_VALUE_STALE ? g->style.warning : g->style.primary,
                                0);
    meter_i18n_format_value(g->text, sizeof(g->text), g->current, missing ? g->state : METER_VALUE_VALID,
                       g->config.unit, 1, g->language);
    lv_label_set_text_static(g->value, g->text);
    lv_obj_align(g->value, LV_ALIGN_BOTTOM_MID, 0, -26);
    /* 未收到数据时读数已显示占位符，不重复占用状态文字区域。 */
    snprintf(g->validity, sizeof(g->validity), "%s",
             g->state == METER_VALUE_UNKNOWN ? "" : meter_i18n_state(g->state));
    lv_label_set_text_static(g->state_label, g->validity);
    lv_obj_align(g->state_label, LV_ALIGN_BOTTOM_MID, 0, -7);
}
static void animate(void *context, int32_t progress)
{
    meter_gauge_t *g = context;
    g->current = g->from + (g->target - g->from) * (progress / 1000.0f);
    redraw(g);
}
meter_gauge_t *meter_gauge_create(lv_obj_t *parent, int x, int y, const meter_gauge_config_t *c)
{
    if (!c || c->min >= c->max || truncf(c->min) != c->min || truncf(c->max) != c->max || c->ticks < 2 ||
        !c->major_every || c->diameter < 50)
        return NULL;
    meter_gauge_t *g = lv_malloc(sizeof(*g));
    if (!g)
        return NULL;
    memset(g, 0, sizeof(*g));
    g->config = *c;
    g->current = c->min;
    g->style = *meter_widget_style_default();
    if (c->diameter < 220)
        g->style.value_font = &lv_font_montserrat_16;
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
    lv_obj_set_style_arc_color(g->scale, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_line_width(g->scale, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(g->scale, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g->scale, 2, LV_PART_INDICATOR);
    lv_obj_set_style_length(g->scale, 7, LV_PART_ITEMS);
    lv_obj_set_style_length(g->scale, 13, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g->scale, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_text_font(g->scale, &lv_font_montserrat_14, LV_PART_INDICATOR);
    g->needle = lv_line_create(g->root);
    lv_obj_set_style_line_width(g->needle, 4, 0);
    lv_obj_set_style_line_rounded(g->needle, true, 0);
    g->hub = lv_obj_create(g->root);
    lv_obj_remove_style_all(g->hub);
    lv_obj_set_size(g->hub, 12, 12);
    lv_obj_center(g->hub);
    lv_obj_set_style_radius(g->hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g->hub, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g->hub, 255, 0);
    g->value = meter_text(g->root, 0, 0, "", g->style.value_font, 0);
    g->state_label = meter_text(g->root, 0, 0, "", g->style.label_font, 0);
    apply_style(g);
    redraw(g);
    return g;
}
bool meter_gauge_set_range(meter_gauge_t *g, float min, float max)
{
    if (!isfinite(min) || !isfinite(max) || min >= max || truncf(min) != min || truncf(max) != max)
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
    redraw(g);
}
void meter_gauge_set_style(meter_gauge_t *g, const meter_widget_style_t *style)
{
    if (style) g->style = *style;
    apply_style(g);
    redraw(g);
}
void meter_gauge_set_value(meter_gauge_t *g, float value)
{
    if (!isfinite(value))
    {
        meter_gauge_set_state(g, METER_VALUE_ERROR);
        return;
    }
    g->current = g->target = fmaxf(g->config.min, fminf(g->config.max, value));
    lv_anim_delete(g, animate);
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
    if (value == g->target)
        return;
    if (!duration)
    {
        meter_gauge_set_value(g, value);
        return;
    }
    lv_anim_delete(g, animate);
    g->from = g->current;
    g->target = value;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g);
    lv_anim_set_exec_cb(&animation, animate);
    lv_anim_set_values(&animation, 0, 1000);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}
void meter_gauge_set_state(meter_gauge_t *g, meter_value_state_t state)
{
    g->state = state;
    redraw(g);
}
void meter_gauge_set_language(meter_gauge_t *g, meter_language_t language)
{
    g->language = language;
    meter_i18n_apply_font(g->value, language, meter_font_role_of(g->style.value_font));
    meter_i18n_apply_font(g->state_label, language, meter_font_role_of(g->style.label_font));
    redraw(g);
}
