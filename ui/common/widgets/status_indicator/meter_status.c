#include "ui/common/widgets/meter_widgets.h"
#include <string.h>
struct meter_status
{
    lv_obj_t *root, *label, *icon;
    const char *name;
    char text[48];
};
static void dispose(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}
meter_status_t *meter_status_create(lv_obj_t *p, int x, int y, const char *name, const lv_image_dsc_t *icon)
{
    meter_status_t *s = lv_malloc(sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->name = name;
    s->root = lv_obj_create(p);
    lv_obj_remove_style_all(s->root);
    lv_obj_set_pos(s->root, x, y);
    lv_obj_set_size(s->root, 140, 30);
    lv_obj_add_event_cb(s->root, dispose, LV_EVENT_DELETE, s);
    if (icon)
    {
        s->icon = lv_image_create(s->root);
        lv_image_set_src(s->icon, icon);
    }
    s->label = meter_text(s->root, 29, 5, name, &lv_font_montserrat_12, 0x9bb2bf);
    return s;
}
void meter_status_set(meter_status_t *s, bool active, meter_value_state_t state)
{
    const char *tag = state == METER_VALUE_UNKNOWN ? " ?"
                      : state == METER_VALUE_ERROR ? " ERR"
                      : state == METER_VALUE_STALE ? " STALE"
                      : active                     ? " ON"
                                                   : " OFF";
    lv_snprintf(s->text, sizeof(s->text), "%s%s", s->name, tag);
    lv_label_set_text_static(s->label, s->text);
    lv_obj_set_style_text_color(
        s->label, lv_color_hex(state == METER_VALUE_VALID ? (active ? 0x5de5ca : 0x7895a8) : 0xf3ba65), 0);
    if (s->icon)
        lv_obj_set_style_image_opa(s->icon, active ? 255 : 100, 0);
}
