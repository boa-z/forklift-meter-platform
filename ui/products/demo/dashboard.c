#include "ui/products/demo/demo_internal.h"
void demo_dashboard_create(demo_ui_t *u)
{
    lv_obj_t *page = u->pages[DEMO_DASHBOARD];
    lv_obj_t *left = demo_panel(page, 16, 0, 316, 302);
    meter_text(left, 17, 12, "01 / TRACTION", &lv_font_montserrat_12, 0x8ba9bb);
    meter_gauge_config_t speed = {0, 50, 135, 405, 26, 5, 256, 85, "km/h"};
    u->speed = meter_gauge_create(left, 29, 38, &speed);
    lv_obj_t *battery = demo_panel(page, 344, 0, 211, 210);
    meter_text(battery, 17, 12, "02 / ENERGY", &lv_font_montserrat_12, 0x8ba9bb);
    u->soc = meter_ring_create(battery, 25, 41, 160, "%");
    meter_ring_set_thresholds(u->soc, 20, 40);
    lv_obj_t *steer = demo_panel(page, 567, 0, 217, 210);
    meter_text(steer, 17, 12, "03 / STEERING", &lv_font_montserrat_12, 0x8ba9bb);
    meter_gauge_config_t steering = {-45, 45, 225, 315, 19, 9, 168, 56, "deg"};
    u->steering = meter_gauge_create(steer, 25, 35, &steering);
    lv_obj_t *height = demo_panel(page, 344, 222, 211, 80);
    meter_text(height, 15, 9, "LIFT HEIGHT", &lv_font_montserrat_12, 0x8ba9bb);
    u->height = meter_linear_meter_create(height, 15, 30, 185, 45, 0, 6, "m");
    lv_obj_t *load = demo_panel(page, 567, 222, 217, 80);
    meter_text(load, 15, 9, "LOAD", &lv_font_montserrat_12, 0x8ba9bb);
    u->load = meter_value_label_create(load, 15, 34, "kg");
    u->load_arc = meter_arc_bar_create(load, 151, 20, 60, 145, 395, 5, "%");
    lv_obj_t *strip = demo_panel(page, 16, 314, 768, 45);
    const char *names[] = {"Seat", "Brake", "Neutral", "Charge", "Warning"};
    const lv_image_dsc_t *icons[] = {&demo_icon_armchair, &demo_icon_circle_letter_p,
                                     &demo_icon_circle_letter_n, &demo_icon_battery,
                                     &demo_icon_alert_triangle};
    for (unsigned i = 0; i < 5; ++i)
        u->status[i] = meter_status_create(strip, 13 + (int)i * 151, 9, names[i], icons[i]);
}
void demo_dashboard_update(demo_ui_t *u, uint32_t dt)
{
    const meter_snapshot_t *s = &u->snapshot;
    float speed = s->signals[METER_SPEED].value * (s->imperial ? 0.621371f : 1);
    meter_gauge_set_range(u->speed, 0, s->imperial ? 31.06855f : 50);
    meter_gauge_set_unit(u->speed, s->imperial ? "mph" : "km/h");
    meter_gauge_set_state(u->speed, s->signals[METER_SPEED].state);
    meter_gauge_set_value_animated(u->speed, speed, 180);
    meter_gauge_advance(u->speed, dt);
    meter_gauge_set_state(u->steering, s->signals[METER_STEERING].state);
    meter_gauge_set_value_animated(u->steering, s->signals[METER_STEERING].value, 180);
    meter_gauge_advance(u->steering, dt);
    meter_ring_set_state(u->soc, s->signals[METER_SOC].state);
    meter_ring_set_value(u->soc, s->signals[METER_SOC].value);
    meter_linear_meter_set_state(u->height, s->signals[METER_HEIGHT].state);
    meter_linear_meter_set_value(u->height, s->signals[METER_HEIGHT].value);
    meter_value_label_set(u->load, s->signals[METER_LOAD].value, s->signals[METER_LOAD].state);
    meter_value_label_advance(u->load, dt);
    meter_ring_set_state(u->load_arc, s->signals[METER_LOAD].state);
    meter_ring_set_value(u->load_arc, s->signals[METER_LOAD].value / 15);
    const meter_signal_id_t ids[] = {METER_SEAT, METER_BRAKE, METER_NEUTRAL, METER_CHARGING, METER_WARNING};
    for (unsigned i = 0; i < 5; ++i)
        meter_status_set(u->status[i], s->signals[ids[i]].value > 0, s->signals[ids[i]].state);
}
