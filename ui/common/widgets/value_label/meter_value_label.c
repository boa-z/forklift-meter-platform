#include "ui/common/formatter/meter_format.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <string.h>
struct meter_value_label
{
    lv_obj_t *label;
    const char *unit;
    float current, target;
    meter_value_state_t state;
    char text[48];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
meter_value_label_t *meter_value_label_create(lv_obj_t *p, int x, int y, const char *unit)
{
    meter_value_label_t *v = lv_malloc(sizeof(*v));
    if (!v)
        return NULL;
    memset(v, 0, sizeof(*v));
    v->unit = unit;
    v->label = meter_text(p, x, y, "--", &lv_font_montserrat_24, 0xedf5f8);
    lv_obj_add_event_cb(v->label, dispose, LV_EVENT_DELETE, v);
    return v;
}
void meter_value_label_set(meter_value_label_t *v, float value, meter_value_state_t state)
{
    if (!isfinite(value))
        state = METER_VALUE_ERROR;
    else
        v->target = value;
    v->state = state;
}
void meter_value_label_advance(meter_value_label_t *v, uint32_t dt)
{
    float t = dt >= 180 ? 1 : dt / 180.0f;
    v->current += (v->target - v->current) * t;
    meter_format_value(v->text, sizeof(v->text), v->current, v->state, v->unit, 0);
    lv_label_set_text_static(v->label, v->text);
}
