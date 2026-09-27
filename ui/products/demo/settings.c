#include "ui/products/demo/demo_internal.h"
static void action(lv_event_t *e)
{
    demo_ui_t *u = lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    meter_action_t a = {METER_ACTION_UNITS, 0, 0};
    if (target == u->unit_button)
        a.value = !u->snapshot.imperial;
    else if (target == u->brightness)
    {
        a.kind = METER_ACTION_BRIGHTNESS;
        a.value = (float)lv_slider_get_value(target);
    }
    else
    {
        a.kind = METER_ACTION_PARAMETER;
        a.id = 1;
        a.value = (float)lv_slider_get_value(target);
    }
    u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &a);
}
void demo_settings_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_SETTINGS];
    meter_text(p, 24, 4, "LOCAL PREFERENCES", &lv_font_montserrat_20, 0xedf5f8);
    meter_text(p, 24, 33, "Demo settings only / vehicle control is disabled", &lv_font_montserrat_12,
               0x8ba9bb);
    lv_obj_t *card = demo_panel(p, 16, 66, 768, 255);
    meter_text(card, 22, 22, "Speed units", &lv_font_montserrat_16, 0xe4eff5);
    u->unit_button = lv_button_create(card);
    lv_obj_set_pos(u->unit_button, 471, 12);
    lv_obj_set_size(u->unit_button, 250, 43);
    lv_obj_set_style_bg_color(u->unit_button, lv_color_hex(0x255448), 0);
    lv_obj_t *label = lv_label_create(u->unit_button);
    lv_label_set_text(label, "Metric / km/h");
    lv_obj_center(label);
    lv_obj_add_event_cb(u->unit_button, action, LV_EVENT_CLICKED, u);
    meter_text(card, 22, 89, "Display brightness", &lv_font_montserrat_16, 0xe4eff5);
    u->brightness = lv_slider_create(card);
    lv_obj_set_pos(u->brightness, 412, 96);
    lv_obj_set_size(u->brightness, 300, 10);
    lv_slider_set_range(u->brightness, 10, 100);
    lv_slider_set_value(u->brightness, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(u->brightness, action, LV_EVENT_RELEASED, u);
    meter_text(card, 22, 154, "Demo speed limit / km/h", &lv_font_montserrat_16, 0xe4eff5);
    u->limit = lv_slider_create(card);
    lv_obj_set_pos(u->limit, 412, 161);
    lv_obj_set_size(u->limit, 300, 10);
    lv_slider_set_range(u->limit, 5, 50);
    lv_slider_set_value(u->limit, 25, LV_ANIM_OFF);
    lv_obj_add_event_cb(u->limit, action, LV_EVENT_RELEASED, u);
    meter_text(card, 22, 211, "Limits are stored examples; synthetic playback is independent.",
               &lv_font_montserrat_12, 0x8ba9bb);
    u->setting_status = meter_text(p, 24, 337, "English / local settings / no maintenance authorization",
                                   &lv_font_montserrat_12, 0x8ba9bb);
}
void demo_settings_update(demo_ui_t *u)
{
    lv_obj_t *label = lv_obj_get_child(u->unit_button, 0);
    lv_label_set_text_static(label, u->snapshot.imperial ? "Imperial / mph" : "Metric / km/h");
    if (!lv_obj_has_state(u->brightness, LV_STATE_PRESSED))
        lv_slider_set_value(u->brightness, u->snapshot.brightness, LV_ANIM_OFF);
    if (!lv_obj_has_state(u->limit, LV_STATE_PRESSED))
        lv_slider_set_value(u->limit, (int)u->snapshot.parameters[0], LV_ANIM_OFF);
    lv_label_set_text_static(u->setting_status,
                             u->action_failed ? "Settings request failed; retry after storage is available."
                                              : "English / local settings / no maintenance authorization");
}
