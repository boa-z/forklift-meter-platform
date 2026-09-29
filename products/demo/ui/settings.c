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
/* 列表只负责路由，实际修改必须发生在独立详情页。 */
static const demo_text_id_t setting_titles[] = {
    DEMO_TXT_SPEED_UNITS, DEMO_TXT_LANGUAGE,   DEMO_TXT_BRIGHTNESS,    DEMO_TXT_SPEED_LIMIT,
    DEMO_TXT_CAN_RATE,    DEMO_TXT_HOUR_METER, DEMO_TXT_SPEED_DISPLAY, DEMO_TXT_MODE_MEMORY};

static void settings_detail_close(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    lv_obj_set_hidden(u->settings_detail, true);
    lv_obj_set_hidden(lv_obj_get_parent(u->settings_pager.indicator), false);
}
static void settings_detail_open(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    unsigned selected = 4 + DEMO_ADMIN_COUNT;
    for (unsigned i = 0; i < 4; ++i)
        if (target == u->setting_entries[i])
            selected = i;
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
        if (target == u->admin_items[i] && u->view.admin_authorized)
            selected = 4 + i;
    if (selected >= 4 + DEMO_ADMIN_COUNT)
        return;
    u->selected_setting = selected;
    lv_obj_t *controls[] = {u->unit_button, u->language_button, u->brightness, u->limit};
    for (unsigned i = 0; i < 4; ++i)
        lv_obj_set_hidden(controls[i], i != selected);
    lv_obj_set_hidden(u->admin_detail_button, selected < 4);
    lv_obj_set_hidden(u->settings_detail, false);
    lv_obj_set_hidden(lv_obj_get_parent(u->settings_pager.indicator), true);
    demo_settings_update(u);
}
void demo_editors_close(demo_ui_t *u)
{
    lv_obj_set_hidden(u->parameter_editor, true);
    lv_obj_set_hidden(u->password_editor, true);
    lv_textarea_set_text(u->user_password, "");
    lv_textarea_set_text(u->admin_password, "");
    if (u->settings_detail)
        lv_obj_set_hidden(u->settings_detail, true);
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
    u->admin_navigation_pending = false;
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
        lv_obj_send_event(u->password_keyboard, LV_EVENT_READY, NULL);
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
    if (u->selected_setting < 4 || u->selected_setting >= 4 + DEMO_ADMIN_COUNT)
        return;
    unsigned i = u->selected_setting - 4;
    unsigned value = (u->view.admin_values[i] + 1u) % (i == 0 ? 3u : 2u);
    meter_action_t intent = demo_admin_intent(i, (float)value);
    u->action_failed = !u->actions.send || !u->actions.send(u->actions.context, &intent);
}
void demo_settings_show_page(demo_ui_t *u, unsigned page)
{
    if (page >= 2)
        return;
    demo_editors_close(u);
    u->settings_tab = page;
    u->admin_navigation_pending = false;
    lv_obj_set_hidden(lv_obj_get_parent(u->settings_pager.indicator), false);
    u->version_open = false;
    lv_obj_set_hidden(u->version_back, true);
    (void)demo_pager_select(&u->settings_pager, 0);
    for (unsigned i = 0; i < 4; ++i)
    {
        lv_obj_set_hidden(u->settings_cards[i], i != (page == 1 ? 2 : page));
        if (i < 2)
            demo_theme_menu_button(u->settings_menu[i], i == page);
    }
}
static void settings_menu_select(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    for (unsigned i = 0; i < 2; ++i)
        if (lv_event_get_target_obj(event) == u->settings_menu[i])
        {
            if (i == 1 && !u->view.admin_authorized)
            {
                demo_editors_close(u);
                u->admin_navigation_pending = true;
                u->password_admin = true;
                lv_textarea_set_text(u->admin_password, "");
                lv_obj_set_hidden(u->user_password, true);
                lv_obj_set_hidden(u->admin_password, false);
                lv_obj_set_hidden(u->password_editor, false);
            }
            else
                demo_settings_show_page(u, i);
        }
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    (void)demo_pager_select(&u->settings_pager, demo_pager_target(&u->settings_pager, event));
}
static void version_open(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    u->version_open = true;
    lv_obj_set_hidden(lv_obj_get_parent(u->settings_pager.indicator), true);
    for (unsigned i = 0; i < 4; ++i)
        lv_obj_set_hidden(u->settings_cards[i], i != 3);
    lv_label_set_text(u->settings_title, demo_i18n_text(DEMO_TXT_INSTRUMENT_VERSION));
    lv_obj_set_hidden(u->version_back, false);
}
static void version_close(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    (void)event;
    lv_obj_set_hidden(u->version_back, true);
    demo_settings_show_page(u, 0);
}
static lv_obj_t *settings_button(demo_ui_t *u, lv_obj_t *parent, int x, int y, demo_text_id_t text,
                                 lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    demo_theme_list_row(button);
    lv_obj_set_style_text_color(button, lv_color_hex(0xedf5f8), 0);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 250, 48);
    lv_obj_t *label = demo_text(u, button, 0, 0, text, &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_center(label);
    if (callback)
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, u);
    return button;
}
void demo_settings_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_SETTINGS];
    u->settings_title = meter_text(p, 430, 8, "", &lv_font_montserrat_24, 0xedf5f8);
    u->settings_note = meter_text(p, 24, 34, "", &lv_font_montserrat_20, 0x8ba9bb);
    lv_obj_set_hidden(u->settings_note, true);
    /* 左侧背景铺满内容区，分类按钮保持固定高度。 */
    u->settings_rail = lv_obj_create(p);
    lv_obj_remove_style_all(u->settings_rail);
    lv_obj_set_pos(u->settings_rail, 0, 0);
    lv_obj_set_size(u->settings_rail, 200, 372);
    lv_obj_set_style_bg_color(u->settings_rail, lv_color_hex(0x1a242b), 0);
    lv_obj_set_style_bg_opa(u->settings_rail, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(u->settings_rail, 0, 0);
    lv_obj_set_style_outline_width(u->settings_rail, 0, 0);
    lv_obj_set_scrollable(u->settings_rail, false);
    const demo_text_id_t menu_ids[] = {DEMO_TXT_USER_SETTINGS, DEMO_TXT_ADMIN_SETTINGS};
    for (unsigned i = 0; i < 2; ++i)
    {
        u->settings_menu[i] = lv_button_create(u->settings_rail);
        demo_theme_menu_button(u->settings_menu[i], i == 0);
        lv_obj_set_pos(u->settings_menu[i], 0, (int)i * 64);
        lv_obj_set_size(u->settings_menu[i], 200, 64);
        lv_obj_add_event_cb(u->settings_menu[i], settings_menu_select, LV_EVENT_CLICKED, u);
        lv_obj_t *label =
            demo_text(u, u->settings_menu[i], 12, 0, menu_ids[i], &lv_font_montserrat_20, 0xedf5f8);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, 176);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        u->settings_cards[i] = lv_obj_create(p);
        lv_obj_remove_style_all(u->settings_cards[i]);
        lv_obj_set_pos(u->settings_cards[i], 200, DEMO_LIST_TOP);
        lv_obj_set_size(u->settings_cards[i], 584, DEMO_LIST_CAPACITY * DEMO_LIST_PITCH);
        lv_obj_set_scrollable(u->settings_cards[i], false);
    }
    lv_obj_t *card = u->settings_cards[0];
    for (unsigned i = 0; i < 4; ++i)
    {
        u->setting_entries[i] =
            settings_button(u, card, 0, (int)i * DEMO_LIST_PITCH, setting_titles[i], settings_detail_open);
        lv_obj_set_size(u->setting_entries[i], 584, DEMO_LIST_HEIGHT);
        lv_obj_t *label = lv_obj_get_child(u->setting_entries[i], 0);
        lv_obj_set_width(label, 280);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 18, 0);
        u->setting_values[i] = meter_text(u->setting_entries[i], 0, 0, "", &lv_font_montserrat_20, 0xedf5f8);
        lv_obj_set_width(u->setting_values[i], 236);
        lv_obj_set_style_text_align(u->setting_values[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(u->setting_values[i], LV_ALIGN_RIGHT_MID, -30, 0);
    }
    u->version_entry =
        settings_button(u, card, 0, 4 * DEMO_LIST_PITCH, DEMO_TXT_INSTRUMENT_VERSION, version_open);
    lv_obj_set_size(u->version_entry, 584, DEMO_LIST_HEIGHT);
    lv_obj_align(lv_obj_get_child(u->version_entry, 0), LV_ALIGN_LEFT_MID, 18, 0);
    /* 详情控件固定归属于详情页，避免搬移对象破坏列表和触摸事件。 */
    u->settings_detail = lv_obj_create(p);
    lv_obj_remove_style_all(u->settings_detail);
    lv_obj_set_pos(u->settings_detail, 200, 0);
    lv_obj_set_size(u->settings_detail, 600, 372);
    lv_obj_set_style_bg_color(u->settings_detail, lv_color_hex(0x07090b), 0);
    lv_obj_set_style_bg_opa(u->settings_detail, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(u->settings_detail, false);
    u->settings_detail_title = meter_text(u->settings_detail, 18, 20, "", &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_set_width(u->settings_detail_title, 470);
    lv_label_set_long_mode(u->settings_detail_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    u->settings_detail_back = lv_button_create(u->settings_detail);
    demo_theme_menu_button(u->settings_detail_back, false);
    lv_obj_set_pos(u->settings_detail_back, 514, 8);
    lv_obj_set_size(u->settings_detail_back, 70, 44);
    lv_obj_t *detail_back_label =
        meter_text(u->settings_detail_back, 0, 0, LV_SYMBOL_LEFT, &lv_font_montserrat_24, 0xedf5f8);
    lv_obj_center(detail_back_label);
    lv_obj_add_event_cb(u->settings_detail_back, settings_detail_close, LV_EVENT_CLICKED, u);
    u->settings_detail_value = meter_text(u->settings_detail, 42, 86, "", &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_set_width(u->settings_detail_value, 500);
    lv_obj_set_style_text_align(u->settings_detail_value, LV_TEXT_ALIGN_CENTER, 0);
    u->unit_button = settings_button(u, u->settings_detail, 166, 160, DEMO_TXT_METRIC, action);
    u->language_button = settings_button(u, u->settings_detail, 166, 160, DEMO_TXT_CHINESE, action);
    u->admin_detail_button =
        settings_button(u, u->settings_detail, 166, 160, DEMO_TXT_EDIT_VALUE, admin_change);
    u->brightness = lv_slider_create(u->settings_detail);
    u->limit = lv_slider_create(u->settings_detail);
    lv_obj_t *sliders[] = {u->brightness, u->limit};
    for (unsigned i = 0; i < 2; ++i)
    {
        demo_theme_slider(sliders[i]);
        lv_obj_set_pos(sliders[i], 70, 180);
        lv_obj_set_size(sliders[i], 444, 16);
        lv_obj_add_event_cb(sliders[i], action, LV_EVENT_RELEASED, u);
    }
    lv_slider_set_range(u->brightness, 10, 100);
    lv_slider_set_range(u->limit, 5, 50);
    lv_obj_set_hidden(u->settings_detail, true);
    card = u->settings_cards[1];
    u->user_password_button = settings_button(u, card, 22, 24, DEMO_TXT_USER_PASSWORD, password_open);
    u->admin_password_button = settings_button(u, card, 290, 24, DEMO_TXT_ADMIN_PASSWORD, password_open);
    u->password_status = meter_text(card, 22, 92, "", &lv_font_montserrat_20, 0x5de5ca);
    u->logout_button = settings_button(u, card, 290, 92, DEMO_TXT_SIGN_OUT, logout);
    /* 参考参考产品密码页：左侧设置导航，右侧标题、输入框和 3x4 数字键盘。 */
    u->password_editor = lv_obj_create(u->root);
    lv_obj_remove_style_all(u->password_editor);
    lv_obj_set_pos(u->password_editor, 0, 53);
    lv_obj_set_size(u->password_editor, 800, 372);
    lv_obj_set_style_bg_color(u->password_editor, lv_color_hex(0x11161b), 0);
    lv_obj_set_style_bg_opa(u->password_editor, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(u->password_editor, false);
    lv_obj_t *side = lv_obj_create(u->password_editor);
    lv_obj_remove_style_all(side);
    lv_obj_set_pos(side, 0, 0);
    lv_obj_set_size(side, 200, 372);
    lv_obj_set_style_bg_color(side, lv_color_hex(0x1a242b), 0);
    lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);

    lv_obj_t *side_system = lv_button_create(side);
    demo_theme_menu_button(side_system, false);
    lv_obj_set_pos(side_system, 0, 0);
    lv_obj_set_size(side_system, 200, 64);
    lv_obj_t *system_label =
        demo_text(u, side_system, 18, 0, DEMO_TXT_USER_SETTINGS, &lv_font_montserrat_20, 0xedf5f8);
    lv_obj_set_width(system_label, 176);
    lv_label_set_long_mode(system_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(system_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(system_label);
    lv_obj_t *side_advanced = lv_button_create(side);
    demo_theme_menu_button(side_advanced, true);
    lv_obj_set_pos(side_advanced, 0, 64);
    lv_obj_set_size(side_advanced, 200, 64);
    lv_obj_t *side_label =
        demo_text(u, side_advanced, 18, 0, DEMO_TXT_ADMIN_SETTINGS, &lv_font_montserrat_20, 0xffffff);
    lv_obj_set_width(side_label, 176);
    lv_label_set_long_mode(side_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(side_label, LV_TEXT_ALIGN_CENTER, 0);
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
        lv_obj_set_style_bg_color(fields[i], lv_color_hex(0x1a242b), 0);
        lv_obj_set_style_text_color(fields[i], lv_color_hex(0xedf5f8), 0);
        lv_obj_set_style_border_color(fields[i], lv_color_hex(0x53616b), 0);
        lv_obj_set_style_border_width(fields[i], 1, 0);
        lv_obj_set_style_radius(fields[i], 6, 0);
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
    u->admin_locked = meter_text(card, 0, 92, LV_SYMBOL_WARNING, &lv_font_montserrat_24, 0xf3ba65);
    lv_obj_set_width(u->admin_locked, 544);
    lv_obj_set_style_text_align(u->admin_locked, LV_TEXT_ALIGN_CENTER, 0);
    const demo_text_id_t admin_ids[] = {DEMO_TXT_CAN_RATE, DEMO_TXT_HOUR_METER, DEMO_TXT_SPEED_DISPLAY,
                                        DEMO_TXT_MODE_MEMORY};
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
    {
        u->admin_items[i] =
            settings_button(u, card, 0, (int)i * DEMO_LIST_PITCH, admin_ids[i], settings_detail_open);
        lv_obj_set_size(u->admin_items[i], 584, DEMO_LIST_HEIGHT);
        lv_obj_t *label = lv_obj_get_child(u->admin_items[i], 0);
        lv_obj_set_width(label, 280);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 18, 0);
        u->admin_value_labels[i] = meter_text(u->admin_items[i], 0, 0, "", &lv_font_montserrat_20, 0xedf5f8);
        lv_obj_set_width(u->admin_value_labels[i], 236);
        lv_obj_set_style_text_align(u->admin_value_labels[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(u->admin_value_labels[i], LV_ALIGN_RIGHT_MID, -30, 0);
        lv_obj_set_hidden(u->admin_items[i], true);
    }
    card = u->settings_cards[3];
    const demo_text_id_t version_ids[] = {DEMO_TXT_FIRMWARE, DEMO_TXT_PRODUCT, DEMO_TXT_FRAMEWORK,
                                          DEMO_TXT_INSTRUMENT_VERSION};
    for (unsigned i = 0; i < 4; ++i)
    {
        u->version_labels[i] = i == 3 ? meter_text(card, 22, 18 + (int)i * DEMO_LIST_PITCH, "LVGL",
                                                   &lv_font_montserrat_20, 0x9cb5c4)
                                      : demo_text(u, card, 22, 18 + (int)i * DEMO_LIST_PITCH, version_ids[i],
                                                  &lv_font_montserrat_20, 0x9cb5c4);
        u->version_values[i] =
            meter_text(card, 290, 18 + (int)i * DEMO_LIST_PITCH, "", &lv_font_montserrat_20, 0xedf5f8);
        lv_obj_set_width(u->version_values[i], 276);
        lv_label_set_long_mode(u->version_values[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    lv_label_set_text(u->version_labels[3], "LVGL");
    u->version_back = lv_button_create(p);
    demo_theme_menu_button(u->version_back, false);
    lv_obj_set_pos(u->version_back, 710, 8);
    lv_obj_set_size(u->version_back, 70, 42);
    lv_obj_t *version_back_label =
        meter_text(u->version_back, 0, 0, LV_SYMBOL_LEFT, &lv_font_montserrat_24, 0xedf5f8);
    lv_obj_center(version_back_label);
    lv_obj_add_event_cb(u->version_back, version_close, LV_EVENT_CLICKED, u);
    lv_obj_set_hidden(u->version_back, true);
    u->setting_status = meter_text(u->root, 218, 393, "", &lv_font_montserrat_16, 0xff856d);

    demo_pager_create(p, &u->settings_pager, 1, turn_page, u);
    lv_obj_move_foreground(u->settings_detail);
    demo_settings_show_page(u, 0);
}
void demo_settings_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN
                                ? &lv_font_montserrat_20
                                : meter_font_get(u->view.language, METER_FONT_LABEL);
    if (u->admin_navigation_pending && u->view.admin_authorized)
    {
        u->admin_navigation_pending = false;
        demo_settings_show_page(u, 1);
    }
    if (!u->view.admin_authorized && u->settings_tab == 1)
        demo_settings_show_page(u, 0);
    lv_label_set_text(u->settings_title, u->version_open ? demo_i18n_text(DEMO_TXT_INSTRUMENT_VERSION) : "");
    lv_label_set_text(lv_obj_get_child(u->unit_button, 0),
                      demo_i18n_text(u->view.imperial ? DEMO_TXT_METRIC : DEMO_TXT_IMPERIAL));
    lv_label_set_text(
        lv_obj_get_child(u->language_button, 0),
        demo_i18n_text(u->view.language == METER_LANGUAGE_EN ? DEMO_TXT_CHINESE : DEMO_TXT_ENGLISH));
    if (!lv_obj_has_state(u->brightness, LV_STATE_PRESSED))
        lv_slider_set_value(u->brightness, u->view.brightness, LV_ANIM_OFF);
    lv_obj_set_state(u->limit, LV_STATE_DISABLED, !u->view.limit_available);
    if (u->view.limit_available && !lv_obj_has_state(u->limit, LV_STATE_PRESSED))
        lv_slider_set_value(u->limit, (int)u->view.limit, LV_ANIM_OFF);
    lv_obj_set_hidden(u->setting_status, u->page != DEMO_SETTINGS);
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
    lv_label_set_text(u->setting_values[0],
                      demo_i18n_text(u->view.imperial ? DEMO_TXT_IMPERIAL : DEMO_TXT_METRIC));
    lv_label_set_text(
        u->setting_values[1],
        demo_i18n_text(u->view.language == METER_LANGUAGE_EN ? DEMO_TXT_ENGLISH : DEMO_TXT_CHINESE));
    lv_label_set_text_fmt(u->setting_values[2], "%u %%", (unsigned)u->view.brightness);
    if (u->view.limit_available)
        lv_label_set_text_fmt(u->setting_values[3], "%.0f km/h", (double)u->view.limit);
    else
        lv_label_set_text(u->setting_values[3], "--");
    for (unsigned i = 0; i < 4; ++i)
        lv_obj_set_style_text_font(u->setting_values[i], font, 0);
    if (u->selected_setting < 4 + DEMO_ADMIN_COUNT)
    {
        lv_label_set_text(u->settings_detail_title, demo_i18n_text(setting_titles[u->selected_setting]));
        lv_obj_t *value = u->selected_setting < 4 ? u->setting_values[u->selected_setting]
                                                  : u->admin_value_labels[u->selected_setting - 4];
        lv_label_set_text(u->settings_detail_value, lv_label_get_text(value));
    }
    lv_obj_t *dynamic[] = {u->settings_title,
                           u->settings_note,
                           u->password_status,
                           u->setting_status,
                           u->settings_detail_title,
                           u->settings_detail_value,
                           lv_obj_get_child(u->unit_button, 0),
                           lv_obj_get_child(u->language_button, 0)};
    for (unsigned i = 0; i < sizeof(dynamic) / sizeof(dynamic[0]); ++i)
        lv_obj_set_style_text_font(dynamic[i], font, 0);
    for (unsigned i = 0; i < 2; ++i)
        lv_obj_set_style_text_font(lv_obj_get_child(u->settings_menu[i], 0), font, 0);
    for (unsigned i = 0; i < 4; ++i)
    {
        lv_obj_set_style_text_font(u->admin_value_labels[i], font, 0);
        lv_obj_set_style_text_font(u->version_values[i], font, 0);
    }
}
