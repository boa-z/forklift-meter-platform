#ifndef METER_WIDGETS_H
#define METER_WIDGETS_H
#include "contracts/meter_domain.h"
#include <lvgl.h>
typedef struct meter_gauge meter_gauge_t;
typedef struct meter_ring meter_ring_t;
typedef struct meter_linear_meter meter_linear_meter_t;
typedef struct meter_status meter_status_t;
typedef struct meter_value_label meter_value_label_t;
typedef struct
{
    float min, max, start_angle, end_angle;
    unsigned ticks, major_every;
    int diameter, needle_length;
    const char *unit;
} meter_gauge_config_t;
meter_gauge_t *meter_gauge_create(lv_obj_t *parent, int x, int y, const meter_gauge_config_t *config);
bool meter_gauge_set_range(meter_gauge_t *g, float min, float max);
void meter_gauge_set_unit(meter_gauge_t *g, const char *unit);
void meter_gauge_set_value(meter_gauge_t *g, float value);
void meter_gauge_set_value_animated(meter_gauge_t *g, float value, uint32_t duration_ms);
void meter_gauge_set_state(meter_gauge_t *g, meter_value_state_t state);
void meter_gauge_advance(meter_gauge_t *g, uint32_t elapsed_ms);
meter_ring_t *meter_ring_create(lv_obj_t *parent, int x, int y, int diameter, const char *unit);
meter_ring_t *meter_arc_bar_create(lv_obj_t *parent, int x, int y, int diameter, float start, float end,
                                   int stroke, const char *unit);
bool meter_ring_set_range(meter_ring_t *r, float min, float max);
void meter_ring_set_thresholds(meter_ring_t *r, float warning, float low);
void meter_ring_set_value(meter_ring_t *r, float value);
void meter_ring_set_state(meter_ring_t *r, meter_value_state_t state);
meter_linear_meter_t *meter_linear_meter_create(lv_obj_t *parent, int x, int y, int width, int height,
                                                float min, float max, const char *unit);
void meter_linear_meter_set_value(meter_linear_meter_t *b, float value);
void meter_linear_meter_set_state(meter_linear_meter_t *b, meter_value_state_t state);
meter_status_t *meter_status_create(lv_obj_t *parent, int x, int y, const char *name,
                                    const lv_image_dsc_t *icon);
void meter_status_set(meter_status_t *s, bool active, meter_value_state_t state);
meter_value_label_t *meter_value_label_create(lv_obj_t *parent, int x, int y, const char *unit);
void meter_value_label_set(meter_value_label_t *v, float value, meter_value_state_t state);
void meter_value_label_advance(meter_value_label_t *v, uint32_t elapsed_ms);
size_t meter_ui_object_count(lv_obj_t *root);
lv_obj_t *meter_text(lv_obj_t *parent, int x, int y, const char *text, const lv_font_t *font, uint32_t color);
#endif
