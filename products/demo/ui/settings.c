#include "ui/demo_internal.h"
static void action(lv_event_t *e)
{
    demo_ui_t *u = lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    meter_action_t a = {METER_ACTION_UNITS, 0, 0};
    if (target == u->unit_button)
        a.value = !u->view.imperial;
    else if (target == u->language_button)
    {
        a.kind = METER_ACTION_LANGUAGE;
        a.value = u->view.language == METER_LANGUAGE_EN ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN;
    }
    else if (target == u->brightness)
    {
        a.kind = METER_ACTION_BRIGHTNESS;
        a.value = (float)lv_slider_get_value(target);
    }
    else
    {
        a = demo_speed_limit_intent((float)lv_slider_get_value(target));
    }
    u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &a);
}
void demo_settings_show_page(demo_ui_t *u, unsigned page)
{
    if (!demo_pager_select(&u->settings_pager, page))
        return;
    for (unsigned i = 0; i < 2; ++i)
        lv_obj_set_hidden(u->settings_cards[i], i != page);
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    demo_settings_show_page(u, demo_pager_target(&u->settings_pager, event));
}
void demo_settings_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_SETTINGS];
    demo_text(u, p, 24, 4, DEMO_TXT_LOCAL_PREFERENCES, &lv_font_montserrat_24, 0xedf5f8);
    demo_text(u, p, 24, 33, DEMO_TXT_SETTINGS_SUBTITLE, &lv_font_montserrat_20, 0x8ba9bb);
    for (unsigned i = 0; i < 2; ++i)
        u->settings_cards[i] = demo_panel(p, 16, 66, 768, 206);
    lv_obj_t *card = u->settings_cards[0];
    demo_text(u, card, 22, 32, DEMO_TXT_SPEED_UNITS, &lv_font_montserrat_20, 0xe4eff5);
    demo_text(u, card, 22, 114, DEMO_TXT_LANGUAGE, &lv_font_montserrat_20, 0xe4eff5);
    u->language_button = lv_button_create(card);
    demo_theme_button(u->language_button);
    lv_obj_set_pos(u->language_button, 471, 102);
    lv_obj_set_size(u->language_button, 250, 48);
    lv_obj_t *language_label = lv_label_create(u->language_button);
    lv_label_set_text(language_label, demo_i18n_text(DEMO_TXT_CHINESE));
    lv_obj_center(language_label);
    lv_obj_add_event_cb(u->language_button, action, LV_EVENT_CLICKED, u);
    u->unit_button = lv_button_create(card);
    demo_theme_button(u->unit_button);
    lv_obj_set_pos(u->unit_button, 471, 20);
    lv_obj_set_size(u->unit_button, 250, 48);
    lv_obj_set_style_bg_color(u->unit_button, lv_color_hex(0x255448), 0);
    lv_obj_t *label = lv_label_create(u->unit_button);
    lv_label_set_text(label, demo_i18n_text(DEMO_TXT_METRIC));
    lv_obj_center(label);
    lv_obj_add_event_cb(u->unit_button, action, LV_EVENT_CLICKED, u);
    card = u->settings_cards[1];
    demo_text(u, card, 22, 32, DEMO_TXT_BRIGHTNESS, &lv_font_montserrat_20, 0xe4eff5);
    u->brightness = lv_slider_create(card);
    demo_theme_slider(u->brightness);
    lv_obj_set_pos(u->brightness, 412, 39);
    lv_obj_set_size(u->brightness, 300, 10);
    lv_slider_set_range(u->brightness, 10, 100);
    lv_slider_set_value(u->brightness, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(u->brightness, action, LV_EVENT_RELEASED, u);
    demo_text(u, card, 22, 114, DEMO_TXT_SPEED_LIMIT, &lv_font_montserrat_20, 0xe4eff5);
    u->limit = lv_slider_create(card);
    demo_theme_slider(u->limit);
    lv_obj_set_pos(u->limit, 412, 121);
    lv_obj_set_size(u->limit, 300, 10);
    lv_slider_set_range(u->limit, 5, 50);
    lv_slider_set_value(u->limit, 25, LV_ANIM_OFF);
    lv_obj_add_event_cb(u->limit, action, LV_EVENT_RELEASED, u);
    demo_text(u, card, 22, 172, DEMO_TXT_LIMITS_NOTE, &lv_font_montserrat_20, 0x8ba9bb);
    u->setting_status = demo_text(u, p, 24, 280, DEMO_TXT_SETTINGS_OK, &lv_font_montserrat_20, 0x8ba9bb);
    demo_pager_create(p, &u->settings_pager, 2, turn_page, u);
    demo_settings_show_page(u, 0);
}
void demo_settings_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN
                               ? &lv_font_montserrat_20
                               : meter_font_get(u->view.language, METER_FONT_LABEL);
    lv_obj_t *label = lv_obj_get_child(u->unit_button, 0);
    lv_label_set_text_static(label, demo_i18n_text(u->view.imperial ? DEMO_TXT_IMPERIAL : DEMO_TXT_METRIC));
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text_static(
        lv_obj_get_child(u->language_button, 0),
        demo_i18n_text(u->view.language == METER_LANGUAGE_EN ? DEMO_TXT_CHINESE : DEMO_TXT_ENGLISH));
    meter_i18n_apply_font(lv_obj_get_child(u->language_button, 0), METER_LANGUAGE_ZH, METER_FONT_LABEL);
    if (!lv_obj_has_state(u->brightness, LV_STATE_PRESSED))
        lv_slider_set_value(u->brightness, u->view.brightness, LV_ANIM_OFF);
    if (u->view.limit_available && !lv_obj_has_state(u->limit, LV_STATE_PRESSED))
    {
        lv_slider_set_value(u->limit, (int)u->view.limit, LV_ANIM_OFF);
    }
    lv_label_set_text_static(
        u->setting_status, demo_i18n_text(u->action_failed ? DEMO_TXT_SETTINGS_ERROR : DEMO_TXT_SETTINGS_OK));
    lv_obj_set_style_text_font(u->setting_status, font, 0);
}
