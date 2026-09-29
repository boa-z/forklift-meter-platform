#ifndef DEMO_THEME_H
#define DEMO_THEME_H
#include "ui/common/widgets/meter_widgets.h"
const meter_widget_style_t *demo_theme_widget_style(void);
void demo_theme_panel(lv_obj_t *panel);
void demo_theme_button(lv_obj_t *button);
/* 左侧分类菜单统一使用相同的状态色和无圆角触摸区。 */
void demo_theme_menu_button(lv_obj_t *button, bool selected);
void demo_theme_slider(lv_obj_t *slider);
void demo_theme_keyboard(lv_obj_t *keyboard);
#endif
