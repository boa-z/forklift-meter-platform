#include "ui/common/formatter/meter_format.h"
#include "ui/demo_internal.h"
#include <math.h>
#include <stdlib.h>

static void parameter_select(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    if (!u->view.admin_authorized)
    {
        lv_label_set_text(u->parameter_status, demo_i18n_text(DEMO_TXT_ADMIN_LOCKED));
        return;
    }
    for (unsigned i = 0; i < DEMO_REMOTE_COUNT; ++i)
    {
        if (lv_event_get_target_obj(event) != u->parameter_rows[i])
            continue;
        u->selected_parameter = i;
        lv_snprintf(u->parameter_input_text, sizeof(u->parameter_input_text), "%.1f", u->view.parameters[i].value);
        lv_textarea_set_text(u->parameter_input, u->parameter_input_text);
        lv_keyboard_set_textarea(u->parameter_keyboard, u->parameter_input);
        lv_obj_set_hidden(u->parameter_editor, false);
    }
}
static void parameter_edit(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_CANCEL)
    {
        lv_obj_set_hidden(u->parameter_editor, true);
        return;
    }
    if (lv_event_get_code(event) != LV_EVENT_READY)
        return;
    const char *text = lv_textarea_get_text(u->parameter_input);
    char *end = NULL;
    float value = strtof(text, &end);
    bool accepted = end != text && *end == 0 && isfinite(value) && u->view.admin_authorized;
    if (accepted)
    {
        meter_action_t intent = demo_remote_intent(u->selected_parameter, value);
        accepted = u->actions.send && u->actions.send(u->actions.context, &intent);
    }
    lv_label_set_text(u->parameter_status, demo_i18n_text(accepted ? DEMO_TXT_ACCESS_QUEUED : DEMO_TXT_ACCESS_INVALID));
    /* 关闭后可看到完整结果；最终成功以 App 快照为准。 */
    lv_obj_set_hidden(u->parameter_editor, true);
}
static void monitor_mode_select(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    u->parameter_mode = lv_event_get_target_obj(event) == u->parameter_mode_button;
    u->monitor_pager.count = u->parameter_mode ? 1 : 2;
    demo_monitor_show_page(u, 0);
}
void demo_monitor_show_page(demo_ui_t *u, unsigned page)
{
    if (!demo_pager_select(&u->monitor_pager, page))
        return;
    lv_obj_set_hidden(u->parameter_editor, true);
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
        lv_obj_set_hidden(lv_obj_get_parent(u->monitor_labels[i]), u->parameter_mode || i / DEMO_MONITORS_PER_PAGE != page);
    for (size_t i = 0; i < DEMO_REMOTE_COUNT; ++i)
        lv_obj_set_hidden(u->parameter_rows[i], !u->parameter_mode);
    lv_obj_set_hidden(u->parameter_status, !u->parameter_mode);
    lv_obj_set_state(u->monitor_mode_button, LV_STATE_CHECKED, !u->parameter_mode);
    lv_obj_set_state(u->parameter_mode_button, LV_STATE_CHECKED, u->parameter_mode);
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    demo_monitor_show_page(u, demo_pager_target(&u->monitor_pager, event));
}
void demo_monitor_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_MONITOR];
    u->monitor_mode_button = lv_button_create(p);
    u->parameter_mode_button = lv_button_create(p);
    lv_obj_t *modes[] = {u->monitor_mode_button, u->parameter_mode_button};
    const demo_text_id_t mode_ids[] = {DEMO_TXT_PARAMETER_MONITOR, DEMO_TXT_CONTROLLER_SETTINGS};
    for (unsigned i = 0; i < 2; ++i)
    {
        demo_theme_button(modes[i]);
        lv_obj_set_pos(modes[i], 16 + (int)i * 390, 4);
        lv_obj_set_size(modes[i], 378, 44);
        lv_obj_t *label = demo_text(u, modes[i], 0, 0, mode_ids[i], &lv_font_montserrat_24, 0xedf5f8);
        if (i == 0)
            u->monitor_mode_label = label;
        else
            u->parameter_mode_label = label;
        lv_obj_center(label);
        lv_obj_add_event_cb(modes[i], monitor_mode_select, LV_EVENT_CLICKED, u);
    }
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        unsigned slot = (unsigned)i % DEMO_MONITORS_PER_PAGE;
        int col = (int)(slot / 4), row = (int)(slot % 4);
        lv_obj_t *r = demo_panel(p, 16 + col * 390, 70 + row * 58, 378, 50);
        u->monitor_labels[i] = meter_text(r, 12, 2, demo_i18n_monitor_label(i), &lv_font_montserrat_20, 0x9cb5c4);
        u->monitor_values[i] = meter_text(r, 12, 26, "--", &lv_font_montserrat_20, 0x5de5ca);
    }
    for (size_t i = 0; i < DEMO_REMOTE_COUNT; ++i)
    {
        lv_obj_t *r = lv_button_create(p);
        demo_theme_button(r);
        lv_obj_set_pos(r, 16, 70 + (int)i * 58);
        lv_obj_set_size(r, 768, 50);
        lv_obj_add_event_cb(r, parameter_select, LV_EVENT_CLICKED, u);
        u->parameter_rows[i] = r;
        u->parameter_labels[i] = demo_text(u, r, 18, 12, (demo_text_id_t)(DEMO_TXT_PARAMETER_0 + i),
                                           &lv_font_montserrat_20, 0x9cb5c4);
        u->parameter_values[i] = meter_text(r, 610, 12, "--", &lv_font_montserrat_20, 0x5de5ca);
    }
    u->parameter_status = meter_text(p, 24, 48, "", &lv_font_montserrat_16, 0x5de5ca);
    u->parameter_editor = demo_panel(u->root, 40, 88, 720, 310);
    lv_obj_set_style_bg_opa(u->parameter_editor, LV_OPA_COVER, 0);
    u->parameter_editor_title = demo_text(u, u->parameter_editor, 22, 20, DEMO_TXT_EDIT_VALUE, &lv_font_montserrat_24, 0xedf5f8);
    u->parameter_input = lv_textarea_create(u->parameter_editor);
    lv_textarea_set_one_line(u->parameter_input, true);
    lv_textarea_set_max_length(u->parameter_input, 12);
    lv_textarea_set_accepted_chars(u->parameter_input, "0123456789.-");
    lv_obj_set_pos(u->parameter_input, 22, 76);
    lv_obj_set_size(u->parameter_input, 278, 54);
    u->parameter_keyboard = lv_keyboard_create(u->parameter_editor);
    demo_theme_keyboard(u->parameter_keyboard);
    lv_keyboard_set_mode(u->parameter_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_pos(u->parameter_keyboard, 326, 60);
    lv_obj_set_size(u->parameter_keyboard, 370, 230);
    lv_obj_add_event_cb(u->parameter_keyboard, parameter_edit, LV_EVENT_READY, u);
    lv_obj_add_event_cb(u->parameter_keyboard, parameter_edit, LV_EVENT_CANCEL, u);
    demo_pager_create(p, &u->monitor_pager, 2, turn_page, u);
    demo_monitor_show_page(u, 0);
}
void demo_monitor_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN ? &lv_font_montserrat_20
                                    : meter_font_get(u->view.language, METER_FONT_LABEL);
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        const demo_monitor_view_t *d = &u->view.monitors[i];
        lv_label_set_text(u->monitor_labels[i], demo_i18n_monitor_label(i));
        lv_obj_set_style_text_font(u->monitor_labels[i], font, 0);
        meter_i18n_format_value(u->monitor_text[i], sizeof(u->monitor_text[i]), d->reading.value,
                                d->reading.state, d->unit, 1, u->view.language);
        lv_obj_set_style_text_font(u->monitor_values[i], font, 0);
        lv_label_set_text_static(u->monitor_values[i], u->monitor_text[i]);
    }
    lv_obj_set_style_text_font(u->monitor_mode_label, font, 0);
    lv_obj_set_style_text_font(u->parameter_mode_label, font, 0);
    for (size_t i = 0; i < DEMO_REMOTE_COUNT; ++i)
    {
        lv_obj_set_style_text_font(u->parameter_labels[i], font, 0);
        if (u->view.parameters[i].available)
            lv_snprintf(u->parameter_text[i], sizeof(u->parameter_text[i]), "%.1f", u->view.parameters[i].value);
        else
            lv_snprintf(u->parameter_text[i], sizeof(u->parameter_text[i]), "--");
        lv_label_set_text_static(u->parameter_values[i], u->parameter_text[i]);
        lv_obj_set_style_text_font(u->parameter_values[i], font, 0);
    }
    demo_text_id_t status = u->view.parameter_feedback == DEMO_FEEDBACK_IDLE
        ? DEMO_TXT_REMOTE_NOTE : demo_i18n_feedback(u->view.parameter_feedback);
    lv_label_set_text(u->parameter_status, demo_i18n_text(status));
    lv_obj_set_style_text_font(u->parameter_status, font, 0);
    if (!u->view.admin_authorized)
        lv_obj_set_hidden(u->parameter_editor, true);
}
