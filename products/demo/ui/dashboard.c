#include "ui/demo_internal.h"
void demo_dashboard_create(demo_ui_t *u)
{
    lv_obj_t *page = u->pages[DEMO_DASHBOARD];
    lv_obj_t *left = demo_panel(page, 16, 0, 316, 302);
    demo_text(u, left, 17, 12, DEMO_TXT_TRACTION, &lv_font_montserrat_16, 0x8ba9bb);
    meter_gauge_config_t speed = {0, 50, 135, 405, 26, 5, 256, 85, "km/h"};
    u->speed = meter_gauge_create(left, 29, 38, &speed);
    lv_obj_t *battery = demo_panel(page, 344, 0, 211, 210);
    demo_text(u, battery, 17, 12, DEMO_TXT_ENERGY, &lv_font_montserrat_16, 0x8ba9bb);
    u->soc = meter_ring_create(battery, 25, 41, 160, "%");
    meter_ring_set_thresholds(u->soc, 20, 40);
    lv_obj_t *steer = demo_panel(page, 567, 0, 217, 210);
    demo_text(u, steer, 17, 12, DEMO_TXT_STEERING, &lv_font_montserrat_16, 0x8ba9bb);
    meter_gauge_config_t steering = {-45, 45, 225, 315, 19, 9, 168, 56, "deg"};
    u->steering = meter_gauge_create(steer, 25, 35, &steering);
    lv_obj_t *height = demo_panel(page, 344, 222, 211, 80);
    demo_text(u, height, 15, 9, DEMO_TXT_LIFT_HEIGHT, &lv_font_montserrat_16, 0x8ba9bb);
    u->height = meter_linear_meter_create(height, 15, 30, 185, 45, 0, 6, "m");
    lv_obj_t *load = demo_panel(page, 567, 222, 217, 80);
    demo_text(u, load, 15, 9, DEMO_TXT_LOAD, &lv_font_montserrat_16, 0x8ba9bb);
    u->load = meter_value_label_create(load, 15, 34, "kg");
    u->load_arc = meter_arc_bar_create(load, 151, 20, 60, 0, 360, 5, "kg");
    lv_obj_t *strip = demo_panel(page, 16, 314, 768, 52);
    const lv_image_dsc_t *icons[] = {&demo_icon_armchair, &demo_icon_circle_letter_p,
                                     &demo_icon_circle_letter_n, &demo_icon_battery,
                                     &demo_icon_alert_triangle};
    for (unsigned i = 0; i < 5; ++i)
        u->status[i] = meter_status_create(strip, 13 + (int)i * 151, 11, "", icons[i]);
    const meter_widget_style_t *style = demo_theme_widget_style();
    meter_gauge_set_style(u->speed, style);
    meter_gauge_set_style(u->steering, style);
    meter_ring_set_style(u->soc, style);
    meter_ring_set_style(u->load_arc, style);
    meter_linear_meter_set_style(u->height, style);
    meter_value_label_set_style(u->load, style);
    for (unsigned i = 0; i < 5; ++i)
        meter_status_set_style(u->status[i], style);
}
void demo_dashboard_update(demo_ui_t *u)
{
    const demo_presentation_t *s = &u->view;
    meter_gauge_set_language(u->speed, s->language);
    meter_gauge_set_language(u->steering, s->language);
    meter_ring_set_language(u->soc, s->language);
    meter_ring_set_language(u->load_arc, s->language);
    meter_linear_meter_set_language(u->height, s->language);
    meter_value_label_set_language(u->load, s->language);
    demo_readout_t speed = s->speed;
    demo_readout_t steering = s->steering;
    demo_readout_t soc = s->soc;
    demo_readout_t height = s->height;
    demo_readout_t load = s->load;
    meter_gauge_set_range(u->speed, 0, s->speed_maximum);
    meter_gauge_set_unit(u->speed, s->speed_unit);
    meter_gauge_set_state(u->speed, speed.state);
    meter_gauge_set_value_animated(u->speed, speed.value, 180);
    meter_gauge_set_state(u->steering, steering.state);
    meter_gauge_set_value_animated(u->steering, steering.value, 180);
    meter_ring_set_state(u->soc, soc.state);
    meter_ring_set_value(u->soc, soc.value);
    meter_linear_meter_set_state(u->height, height.state);
    meter_linear_meter_set_value(u->height, height.value);
    meter_value_label_set(u->load, load.value, load.state);
    meter_ring_set_state(u->load_arc, load.state);
    meter_ring_set_range(u->load_arc, 0, 1500);
    meter_ring_set_value(u->load_arc, load.value);
    const demo_text_id_t names[] = {DEMO_TXT_SEAT, DEMO_TXT_BRAKE, DEMO_TXT_NEUTRAL, DEMO_TXT_CHARGE,
                                    DEMO_TXT_WARNING};
    for (unsigned i = 0; i < 5; ++i)
    {
        demo_readout_t flag = s->status[i];
        meter_status_set(u->status[i], flag.value > 0, flag.state);
        meter_status_set_name(u->status[i], demo_i18n_text(names[i]));
        meter_status_set_language(u->status[i], s->language);
    }
}
