#include "ui/demo_internal.h"
#include <stdlib.h>
#include <string.h>

static void action(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    meter_action_t intent = {METER_ACTION_UNITS, 0, !u->view.imperial};
    if (target == u->language_button)
    {
        intent.kind = METER_ACTION_LANGUAGE;
        intent.value = u->view.language == METER_LANGUAGE_EN ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN;
    }
    else if (target == u->brightness)
        intent = (meter_action_t){METER_ACTION_BRIGHTNESS, 0, (float)lv_slider_get_value(target)};
    else if (target == u->limit)
        intent = demo_speed_limit_intent((float)lv_slider_get_value(target));
    u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &intent);
}
void demo_editors_close(demo_ui_t *u)
{
    lv_obj_set_hidden(u->parameter_editor, true);
    lv_obj_set_hidden(u->password_editor, true);
    lv_textarea_set_text(u->user_password, "");
    lv_textarea_set_text(u->admin_password, "");
}
static void password_open(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    u->password_admin = lv_event_get_target_obj(event) == u->admin_password_button;
    lv_obj_set_hidden(u->user_password, u->password_admin);
    lv_obj_set_hidden(u->admin_password, !u->password_admin);
    lv_obj_t *field = u->password_admin ? u->admin_password : u->user_password;
    lv_textarea_set_text(field, "");
    lv_obj_set_hidden(u->password_editor, false);
}
static void password_submit(lv_event_t *event);
static void password_back(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    (void)event;
    demo_editors_close(u);
}
static void password_keypad(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED)
        return;
    lv_obj_t *field = u->password_admin ? u->admin_password : u->user_password;
    const char *key = lv_buttonmatrix_get_button_text(
        lv_event_get_target_obj(event), lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(event)));
    if (!key)
        return;
    if (!strcmp(key, LV_SYMBOL_BACKSPACE))
        lv_textarea_delete_char(field);
    else if (!strcmp(key, LV_SYMBOL_OK))
        password_submit(event);
    else if (strlen(key) == 1u && key[0] >= '0' && key[0] <= '9' && strlen(lv_textarea_get_text(field)) < 4u)
        lv_textarea_add_text(field, key);
}
static void password_submit(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_CANCEL)
    {
        demo_editors_close(u);
        return;
    }
    if (lv_event_get_code(event) != LV_EVENT_READY)
        return;
    const char *text = lv_textarea_get_text(u->password_admin ? u->admin_password : u->user_password);
    bool valid = strlen(text) == 4;
    for (size_t i = 0; valid && i < 4; ++i)
        valid = text[i] >= '0' && text[i] <= '9';
    if (valid)
    {
        meter_action_t intent =
            demo_settings_intent(u->password_admin ? DEMO_INTENT_ADMIN_LOGIN : DEMO_INTENT_USER_LOGIN,
                                 (float)strtoul(text, NULL, 10));
        u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &intent);
    }
    else
        u->action_failed = true;
    demo_editors_close(u);
}
static void logout(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    meter_action_t intent = demo_settings_intent(DEMO_INTENT_LOGOUT, 0);
    u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &intent);
    demo_editors_close(u);
}
static void admin_change(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    if (!u->view.admin_authorized)
        return;
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
        if (lv_event_get_target_obj(event) == u->admin_items[i])
        {
            unsigned value = (u->view.admin_values[i] + 1u) % (i == 0 ? 3u : 2u);
            meter_action_t intent = demo_admin_intent(i, (float)value);
            u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &intent);
        }
}
void demo_settings_show_page(demo_ui_t *u, unsigned page)
{
    if (!demo_pager_select(&u->settings_pager, page))
        return;
    demo_editors_close(u);
    for (unsigned i = 0; i < 4; ++i)
        lv_obj_set_hidden(u->settings_cards[i], i != page);
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    demo_settings_show_page(u, demo_pager_target(&u->settings_pager, event));
}
static lv_obj_t *settings_button(demo_ui_t *u, lv_obj_t *parent, int x, int y, demo_text_id_t text,
                                 lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    demo_theme_button(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 300, 48);
    lv_obj_t *label = demo_text(u, button, 0, 0, text, &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, u);
    return button;
}
void demo_settings_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_SETTINGS];
    u->settings_title = meter_text(p, 24, 4, "", &lv_font_montserrat_24, 0xedf5f8);
    u->settings_note = meter_text(p, 24, 34, "", &lv_font_montserrat_20, 0x8ba9bb);
    for (unsigned i = 0; i < 4; ++i)
        u->settings_cards[i] = demo_panel(p, 16, 66, 768, 240);
    lv_obj_t *card = u->settings_cards[0];
    const demo_text_id_t labels[] = {DEMO_TXT_SPEED_UNITS, DEMO_TXT_LANGUAGE, DEMO_TXT_BRIGHTNESS,
                                     DEMO_TXT_SPEED_LIMIT};
    for (unsigned i = 0; i < 4; ++i)
        demo_text(u, card, 22, 18 + (int)i * 57, labels[i], &lv_font_montserrat_20, 0xe4eff5);
    u->unit_button = settings_button(u, card, 442, 8, DEMO_TXT_METRIC, action);
    u->language_button = settings_button(u, card, 442, 65, DEMO_TXT_CHINESE, action);
    u->brightness = lv_slider_create(card);
    u->limit = lv_slider_create(card);
    lv_obj_t *sliders[] = {u->brightness, u->limit};
    for (unsigned i = 0; i < 2; ++i)
    {
        demo_theme_slider(sliders[i]);
        lv_obj_set_pos(sliders[i], 454, 144 + (int)i * 57);
        lv_obj_set_size(sliders[i], 270, 12);
        lv_obj_add_event_cb(sliders[i], action, LV_EVENT_RELEASED, u);
    }
    lv_slider_set_range(u->brightness, 10, 100);
    lv_slider_set_range(u->limit, 5, 50);
    card = u->settings_cards[1];
    demo_text(u, card, 22, 16, DEMO_TXT_DEMO_CREDENTIALS, &lv_font_montserrat_20, 0x9cb5c4);
    u->user_password_button = settings_button(u, card, 22, 60, DEMO_TXT_USER_PASSWORD, password_open);
    u->admin_password_button = settings_button(u, card, 432, 60, DEMO_TXT_ADMIN_PASSWORD, password_open);
    u->password_status = meter_text(card, 22, 136, "", &lv_font_montserrat_20, 0x5de5ca);
    u->logout_button = settings_button(u, card, 432, 126, DEMO_TXT_SIGN_OUT, logout);
    demo_text(u, card, 22, 194, DEMO_TXT_DEMO_SESSION, &lv_font_montserrat_20, 0x9cb5c4);
    /* 参考杭叉密码页：左侧设置导航，右侧标题、输入框和 3x4 数字键盘。 */
    u->password_editor = lv_obj_create(u->root);
    lv_obj_remove_style_all(u->password_editor);
    lv_obj_set_pos(u->password_editor, 0, 53);
    lv_obj_set_size(u->password_editor, 800, 372);
    lv_obj_set_style_bg_color(u->password_editor, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(u->password_editor, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(u->password_editor, false);
    lv_obj_t *side = lv_obj_create(u->password_editor);
    lv_obj_remove_style_all(side);
    lv_obj_set_pos(side, 0, 0);
    lv_obj_set_size(side, 286, 372);
    lv_obj_set_style_bg_color(side, lv_color_hex(0x202832), 0);
    lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);
    lv_obj_t *side_title = meter_text(side, 22, 30, "", &lv_font_montserrat_24, 0xedf5f8);
    lv_label_set_text(side_title, demo_i18n_text(DEMO_TXT_SETTINGS));
    lv_obj_t *side_system = lv_button_create(side);
    demo_theme_button(side_system);
    lv_obj_set_pos(side_system, 12, 58);
    lv_obj_set_size(side_system, 262, 58);
    lv_obj_t *system_label =
        demo_text(u, side_system, 18, 0, DEMO_TXT_USER_SETTINGS, &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_center(system_label);
    lv_obj_t *side_advanced = lv_button_create(side);
    demo_theme_button(side_advanced);
    lv_obj_set_pos(side_advanced, 12, 126);
    lv_obj_set_size(side_advanced, 262, 58);
    lv_obj_set_style_bg_color(side_advanced, lv_color_hex(0x008f4c), 0);
    lv_obj_t *side_label =
        demo_text(u, side_advanced, 18, 0, DEMO_TXT_ADMIN_SETTINGS, &lv_font_montserrat_20, 0xffffff);
    lv_obj_center(side_label);
    u->password_editor_title =
        demo_text(u, u->password_editor, 320, 16, DEMO_TXT_ENTER_PIN, &lv_font_montserrat_24, 0xedf5f8);
    lv_obj_t *back = lv_button_create(u->password_editor);
    demo_theme_button(back);
    lv_obj_set_pos(back, 710, 10);
    lv_obj_set_size(back, 70, 42);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_t *back_label = meter_text(back, 0, 0, LV_SYMBOL_LEFT, &lv_font_montserrat_24, 0xedf5f8);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, password_back, LV_EVENT_CLICKED, u);
    u->user_password = lv_textarea_create(u->password_editor);
    u->admin_password = lv_textarea_create(u->password_editor);
    lv_obj_t *fields[] = {u->user_password, u->admin_password};
    for (unsigned i = 0; i < 2; ++i)
    {
        lv_textarea_set_one_line(fields[i], true);
        lv_textarea_set_password_mode(fields[i], true);
        lv_textarea_set_password_show_time(fields[i], 0);
        lv_textarea_set_accepted_chars(fields[i], "0123456789");
        lv_textarea_set_max_length(fields[i], 4);
        lv_obj_set_pos(fields[i], 320, 56);
        lv_obj_set_size(fields[i], 400, 58);
        lv_obj_set_style_text_align(fields[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(fields[i], &lv_font_montserrat_24, 0);
    }
    u->password_keyboard = lv_buttonmatrix_create(u->password_editor);
    demo_theme_keyboard(u->password_keyboard);
    static const char *const password_map[] = {
        "1", "2",          "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE,
        "0", LV_SYMBOL_OK, ""};
    lv_buttonmatrix_set_map(u->password_keyboard, password_map);
    lv_obj_set_pos(u->password_keyboard, 320, 126);
    lv_obj_set_size(u->password_keyboard, 400, 230);
    lv_obj_add_event_cb(u->password_keyboard, password_keypad, LV_EVENT_VALUE_CHANGED, u);
    lv_obj_add_event_cb(u->password_keyboard, password_submit, LV_EVENT_READY, u);
    lv_obj_add_event_cb(u->password_keyboard, password_submit, LV_EVENT_CANCEL, u);
    card = u->settings_cards[2];
    u->admin_locked = demo_text(u, card, 22, 92, DEMO_TXT_ADMIN_LOCKED, &lv_font_montserrat_24, 0xf3ba65);
    const demo_text_id_t admin_ids[] = {DEMO_TXT_CAN_RATE, DEMO_TXT_HOUR_METER, DEMO_TXT_SPEED_DISPLAY,
                                        DEMO_TXT_MODE_MEMORY};
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
    {
        u->admin_items[i] = settings_button(u, card, 12, 6 + (int)i * 58, admin_ids[i], admin_change);
        lv_obj_set_width(u->admin_items[i], 744);
        lv_obj_t *label = lv_obj_get_child(u->admin_items[i], 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
        u->admin_value_labels[i] =
            meter_text(u->admin_items[i], 490, 12, "", &lv_font_montserrat_20, 0x5de5ca);
        lv_obj_set_hidden(u->admin_items[i], true);
    }
    card = u->settings_cards[3];
    const demo_text_id_t version_ids[] = {DEMO_TXT_FIRMWARE, DEMO_TXT_PRODUCT, DEMO_TXT_FRAMEWORK,
                                          DEMO_TXT_INSTRUMENT_VERSION};
    for (unsigned i = 0; i < 4; ++i)
    {
        u->version_labels[i] =
            i == 3
                ? meter_text(card, 22, 18 + (int)i * 57, "LVGL", &lv_font_montserrat_20, 0x9cb5c4)
                : demo_text(u, card, 22, 18 + (int)i * 57, version_ids[i], &lv_font_montserrat_20, 0x9cb5c4);
        u->version_values[i] = meter_text(card, 370, 18 + (int)i * 57, "", &lv_font_montserrat_20, 0x5de5ca);
    }
    lv_label_set_text(u->version_labels[3], "LVGL");
    u->setting_status = meter_text(p, 540, 8, "", &lv_font_montserrat_16, 0xff856d);
    demo_pager_create(p, &u->settings_pager, 4, turn_page, u);
    demo_settings_show_page(u, 0);
}
void demo_settings_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN
                                ? &lv_font_montserrat_20
                                : meter_font_get(u->view.language, METER_FONT_LABEL);
    const demo_text_id_t titles[] = {DEMO_TXT_USER_SETTINGS, DEMO_TXT_PASSWORD, DEMO_TXT_ADMIN_SETTINGS,
                                     DEMO_TXT_INSTRUMENT_VERSION};
    lv_label_set_text(u->settings_title, demo_i18n_text(titles[u->settings_pager.current]));
    lv_label_set_text(
        u->settings_note,
        demo_i18n_text(u->settings_pager.current == 2 ? DEMO_TXT_ADMIN_NOTE : DEMO_TXT_SETTINGS_OK));
    lv_label_set_text(lv_obj_get_child(u->unit_button, 0),
                      demo_i18n_text(u->view.imperial ? DEMO_TXT_IMPERIAL : DEMO_TXT_METRIC));
    lv_label_set_text(
        lv_obj_get_child(u->language_button, 0),
        demo_i18n_text(u->view.language == METER_LANGUAGE_EN ? DEMO_TXT_CHINESE : DEMO_TXT_ENGLISH));
    if (!lv_obj_has_state(u->brightness, LV_STATE_PRESSED))
        lv_slider_set_value(u->brightness, u->view.brightness, LV_ANIM_OFF);
    if (u->view.limit_available && !lv_obj_has_state(u->limit, LV_STATE_PRESSED))
        lv_slider_set_value(u->limit, (int)u->view.limit, LV_ANIM_OFF);
    lv_label_set_text(u->setting_status, u->action_failed ? demo_i18n_text(DEMO_TXT_ACCESS_DENIED) : "");
    demo_text_id_t access = u->view.admin_authorized  ? DEMO_TXT_ACCESS_ADMIN
                            : u->view.user_authorized ? DEMO_TXT_ACCESS_USER
                                                      : demo_i18n_feedback(u->view.auth_feedback);
    lv_label_set_text(u->password_status, demo_i18n_text(access));
    lv_obj_set_hidden(u->admin_locked, u->view.admin_authorized);
    const char *rates[] = {"125 kbit/s", "250 kbit/s", "500 kbit/s"};
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
    {
        lv_obj_set_hidden(u->admin_items[i], !u->view.admin_authorized);
        unsigned value = u->view.admin_values[i];
        const char *text = i == 0   ? rates[value < 3 ? value : 0]
                           : i == 1 ? demo_i18n_text(value ? DEMO_TXT_POWER_MODE : DEMO_TXT_WORK_MODE)
                           : i == 2 ? (value ? "1 km/h" : "0.1 km/h")
                                    : demo_i18n_text(value ? DEMO_TXT_REMEMBER : DEMO_TXT_RESET_MODE);
        lv_label_set_text(u->admin_value_labels[i], text);
    }
    lv_label_set_text(u->version_values[0], u->view.firmware_version ? u->view.firmware_version
                                                                     : demo_i18n_text(DEMO_TXT_HOST_BUILD));
    lv_label_set_text(u->version_values[1], "reference-demo");
    lv_label_set_text_fmt(u->version_values[2], "%.8s%s", u->view.framework_revision,
                          strstr(u->view.framework_revision, "dirty") ? " *" : "");
    lv_label_set_text_fmt(u->version_values[3], "%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                          LVGL_VERSION_PATCH);
    lv_obj_t *dynamic[] = {u->settings_title,
                           u->settings_note,
                           u->password_status,
                           u->setting_status,
                           lv_obj_get_child(u->unit_button, 0),
                           lv_obj_get_child(u->language_button, 0)};
    for (unsigned i = 0; i < sizeof(dynamic) / sizeof(dynamic[0]); ++i)
        lv_obj_set_style_text_font(dynamic[i], font, 0);
    for (unsigned i = 0; i < 4; ++i)
    {
        lv_obj_set_style_text_font(u->admin_value_labels[i], font, 0);
        lv_obj_set_style_text_font(u->version_values[i], font, 0);
    }
}
