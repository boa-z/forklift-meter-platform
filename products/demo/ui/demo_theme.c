#include "ui/demo_theme.h"
static const meter_widget_style_t style = {
    .primary = LV_COLOR_MAKE(0xff, 0x8a, 0x00),
    .track = LV_COLOR_MAKE(0x2c, 0x35, 0x3d),
    .text = LV_COLOR_MAKE(0xed, 0xf5, 0xf8),
    .muted = LV_COLOR_MAKE(0xa5, 0xaf, 0xb8),
    .warning = LV_COLOR_MAKE(0xff, 0xa6, 0x00),
    .error = LV_COLOR_MAKE(0xf2, 0x3b, 0x3b),
    .value_font = &lv_font_montserrat_24,
    .label_font = &lv_font_montserrat_16,
};
const meter_widget_style_t *demo_theme_widget_style(void)
{
    return &style;
}
void demo_theme_panel(lv_obj_t *panel)
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x11161b), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x34404a), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_outline_color(panel, lv_color_hex(0x34404a), 0);
    lv_obj_set_style_outline_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_scrollable(panel, false);
}
void demo_theme_button(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1a242b), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xff7a00), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x27343d), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, style.text, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_radius(button, 6, 0);
}
void demo_theme_slider(lv_obj_t *slider)
{
    lv_obj_set_style_bg_color(slider, style.track, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, style.primary, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, style.primary, LV_PART_KNOB);
}

void demo_theme_keyboard(lv_obj_t *keyboard)
{
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x11161b), 0);
    lv_obj_set_style_border_color(keyboard, lv_color_hex(0x4b5861), 0);
    lv_obj_set_style_border_width(keyboard, 1, 0);
    lv_obj_set_style_radius(keyboard, 8, 0);
    lv_obj_set_style_pad_all(keyboard, 8, 0);
    lv_obj_set_style_pad_row(keyboard, 6, 0);
    lv_obj_set_style_pad_column(keyboard, 6, 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x222c33), LV_PART_ITEMS);
    lv_obj_set_style_border_color(keyboard, lv_color_hex(0x53616b), LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 6, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(0xf4f7f8), LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xff7a00), LV_PART_ITEMS | LV_STATE_PRESSED);
}
