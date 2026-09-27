#include "ui/products/demo/demo_theme.h"
static const meter_widget_style_t style = {
    .primary = LV_COLOR_MAKE(0x5d, 0xe5, 0xca), .track = LV_COLOR_MAKE(0x26, 0x3e, 0x4e),
    .text = LV_COLOR_MAKE(0xed, 0xf5, 0xf8), .muted = LV_COLOR_MAKE(0x82, 0x99, 0xa9),
    .warning = LV_COLOR_MAKE(0xf3, 0xba, 0x65), .error = LV_COLOR_MAKE(0xff, 0x85, 0x6d),
    .value_font = &lv_font_montserrat_24, .label_font = &lv_font_montserrat_12,
};
const meter_widget_style_t *demo_theme_widget_style(void) { return &style; }
void demo_theme_panel(lv_obj_t *panel)
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x142a38), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_scrollable(panel, false);
}
void demo_theme_button(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(0x142a38), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x245b50), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button, style.text, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
}
void demo_theme_slider(lv_obj_t *slider)
{
    lv_obj_set_style_bg_color(slider, style.track, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, style.primary, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, style.primary, LV_PART_KNOB);
}
