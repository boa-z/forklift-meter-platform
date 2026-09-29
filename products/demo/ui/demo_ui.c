#include "ui/demo_internal.h"
#include <string.h>
lv_obj_t *demo_text(demo_ui_t *u, lv_obj_t *parent, int x, int y, demo_text_id_t id, const lv_font_t *font,
                    uint32_t color)
{
    (void)u;
    lv_obj_t *label = meter_text(parent, x, y, demo_i18n_text(id), font, color);
    demo_i18n_bind_label(label, id);
    return label;
}
lv_obj_t *demo_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    demo_theme_panel(p);
    return p;
}
bool demo_pager_select(demo_pager_t *pager, unsigned page)
{
    if (page >= pager->count)
        return false;
    pager->current = page;
    /* 页码使用 LVGL 自有字符串，不能借用实时读数的静态缓冲区。 */
    lv_label_set_text_fmt(pager->indicator, "%u / %u", page + 1, pager->count);
    lv_obj_center(pager->indicator);
    lv_obj_set_state(pager->previous, LV_STATE_DISABLED, page == 0);
    lv_obj_set_state(pager->next, LV_STATE_DISABLED, page + 1 == pager->count);
    return true;
}
unsigned demo_pager_target(const demo_pager_t *pager, lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == pager->previous && pager->current > 0)
        return pager->current - 1;
    if (target == pager->next && pager->current + 1 < pager->count)
        return pager->current + 1;
    return pager->current;
}
void demo_pager_create(lv_obj_t *parent, demo_pager_t *pager, unsigned count,
                       lv_event_cb_t callback, void *context)
{
    lv_obj_t *bar = demo_panel(parent, 16, 312, 768, 48);
    pager->count = count;
    pager->previous = lv_button_create(bar);
    pager->next = lv_button_create(bar);
    lv_obj_t *buttons[] = {pager->previous, pager->next};
    const char *labels[] = {"<", ">"};
    for (unsigned i = 0; i < 2; ++i)
    {
        demo_theme_button(buttons[i]);
        lv_obj_set_pos(buttons[i], i == 0 ? 4 : 652, 2);
        lv_obj_set_size(buttons[i], 112, 44);
        lv_obj_set_style_bg_color(buttons[i], lv_color_hex(0x245b50), 0);
        lv_obj_set_style_opa(buttons[i], LV_OPA_40, LV_STATE_DISABLED);
        lv_obj_t *label = meter_text(buttons[i], 0, 0, labels[i], &lv_font_montserrat_24, 0xedf5f8);
        lv_obj_center(label);
        lv_obj_add_event_cb(buttons[i], callback, LV_EVENT_CLICKED, context);
    }
    pager->indicator = meter_text(bar, 0, 0, "", &lv_font_montserrat_20, 0xedf5f8);
    (void)demo_pager_select(pager, 0);
}
void *demo_ui_create(void *parent, const meter_ui_actions_t *actions)
{
    demo_ui_t *u = lv_malloc(sizeof(*u));
    if (!u)
        return NULL;
    memset(u, 0, sizeof(*u));
    if (actions)
        u->actions = *actions;
    u->root = lv_obj_create(parent);
    lv_obj_remove_style_all(u->root);
    lv_obj_set_pos(u->root, 0, 0);
    lv_obj_set_size(u->root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(u->root, lv_color_hex(0x091a25), 0);
    lv_obj_set_style_bg_opa(u->root, 255, 0);
    lv_obj_set_scrollable(u->root, false);
    demo_theme_panel(u->root);
    /* 显示画布铺满矩形屏幕，仅卡片保留圆角。 */
    lv_obj_set_style_radius(u->root, 0, 0);
    demo_text(u, u->root, 22, 15, DEMO_TXT_FIELD, &lv_font_montserrat_24, 0x5de5ca);
    demo_text(u, u->root, 111, 20, DEMO_TXT_REFERENCE, &lv_font_montserrat_16, 0x9cb5c4);
    u->connection = demo_text(u, u->root, 491, 18, DEMO_TXT_WAITING, &lv_font_montserrat_16, 0xf3ba65);
    u->clock = meter_text(u->root, 714, 18, "00:00", &lv_font_montserrat_16, 0xe9f2f5);
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
    {
        u->pages[i] = lv_obj_create(u->root);
        lv_obj_remove_style_all(u->pages[i]);
        lv_obj_set_pos(u->pages[i], 0, 53);
        lv_obj_set_size(u->pages[i], 800, 372);
        lv_obj_set_scrollable(u->pages[i], false);
        demo_theme_panel(u->pages[i]);
        lv_obj_set_style_radius(u->pages[i], 0, 0);
    }
    demo_dashboard_create(u);
    demo_monitor_create(u);
    demo_faults_create(u);
    demo_settings_create(u);
    demo_navigation_create(u);
    lv_obj_move_foreground(u->parameter_editor);
    lv_obj_move_foreground(u->password_editor);
    demo_navigation_show(u, 0);
    meter_update_widget_create(&u->update_widget, u->root);
    return u;
}
void demo_ui_present(void *context, const meter_snapshot_t *snapshot, uint32_t elapsed)
{
    demo_ui_t *u = context;
    (void)elapsed;
    demo_presentation_build(snapshot, &u->view);
    if (!u->language_presented || u->presented_language != u->view.language)
    {
        lv_translation_set_language(meter_i18n_language_code(u->view.language));
        u->language_presented = true;
        u->presented_language = u->view.language;
    }
    const demo_text_id_t states[] = {DEMO_TXT_CONNECTED, DEMO_TXT_OFFLINE, DEMO_TXT_STALE, DEMO_TXT_WAITING};
    demo_text_id_t state = states[u->view.link];
    strcpy(u->connection_text, demo_i18n_text(state));
    lv_label_set_text_static(u->connection, u->connection_text);
    meter_i18n_apply_font(u->connection, u->view.language, METER_FONT_LABEL);
    uint32_t sec = lv_tick_get() / 1000;
    lv_snprintf(u->clock_text, sizeof(u->clock_text), "%02u:%02u", (unsigned)(sec / 60 % 60),
                (unsigned)(sec % 60));
    lv_label_set_text_static(u->clock, u->clock_text);
    demo_dashboard_update(u);
    demo_monitor_update(u);
    demo_faults_update(u);
    demo_settings_update(u);
    lv_obj_set_style_opa(u->pages[u->page], (lv_opa_t)(80 + u->view.brightness * 175 / 100), 0);
}
void demo_ui_destroy(void *context)
{
    demo_ui_t *u = context;
    if (!u)
        return;
    lv_obj_delete(u->root);
    lv_free(u);
}
unsigned demo_ui_active_page(const void *context)
{
    return ((const demo_ui_t *)context)->page;
}
unsigned demo_ui_active_subpage(const void *context)
{
    const demo_ui_t *u = context;
    switch (u->page)
    {
        case DEMO_MONITOR: return u->monitor_pager.current;
        case DEMO_FAULTS: return u->fault_pager.current;
        case DEMO_SETTINGS: return u->settings_pager.current;
        default: return 0;
    }
}

static void present_update(void *context, const meter_update_view_t *view, meter_language_t language,
                           bool preview)
{
    demo_ui_t *u = context;
    const demo_text_id_t phases[] = {
        DEMO_TXT_OTA_IDLE,      DEMO_TXT_OTA_DOWNLOADING, DEMO_TXT_OTA_TRANSFERRED, DEMO_TXT_OTA_VERIFYING,
        DEMO_TXT_OTA_CANDIDATE, DEMO_TXT_OTA_DURABLE,     DEMO_TXT_OTA_ACTIVATING,  DEMO_TXT_OTA_ACTIVATED,
        DEMO_TXT_OTA_CONFIRMED, DEMO_TXT_OTA_ABORTED,     DEMO_TXT_OTA_FAILED};
    demo_text_id_t phase = DEMO_TXT_OTA_FAILED;
    if ((unsigned)view->state < sizeof(phases) / sizeof(phases[0]))
        phase = phases[view->state];
    lv_translation_set_language(meter_i18n_language_code(language));
    meter_update_widget_text_t text = {
        demo_i18n_text(DEMO_TXT_OTA_TITLE),
        demo_i18n_text(phase),
        demo_i18n_text(view->state == METER_UPDATE_ABORTED || view->state == METER_UPDATE_FAILED
                           ? DEMO_TXT_OTA_STOPPED
                           : DEMO_TXT_OTA_NOTE),
        demo_i18n_text(DEMO_TXT_OTA_CURRENT),
        demo_i18n_text(DEMO_TXT_OTA_TARGET),
        demo_i18n_text(DEMO_TXT_OTA_ERROR),
        preview ? demo_i18n_text(DEMO_TXT_OTA_PREVIEW) : ""};
    meter_update_widget_present(&u->update_widget, view, &text, language);
}
void demo_ui_update(void *ui, const meter_update_view_t *view, meter_language_t language)
{
    present_update(ui, view, language, false);
}
void demo_ui_update_preview(void *ui, const meter_update_view_t *view, meter_language_t language)
{
    present_update(ui, view, language, true);
}
