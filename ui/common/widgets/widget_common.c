#include "ui/common/widgets/meter_widgets.h"
lv_obj_t *meter_text(lv_obj_t *parent, int x, int y, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_CLIP);
    return o;
}
size_t meter_ui_object_count(lv_obj_t *root)
{
    size_t n = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        n += meter_ui_object_count(lv_obj_get_child(root, (int32_t)i));
    return n;
}
