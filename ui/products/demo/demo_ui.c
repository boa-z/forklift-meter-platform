#include "ui/products/demo/demo_internal.h"
#include <string.h>
lv_obj_t *demo_text(demo_ui_t *u, lv_obj_t *parent, int x, int y, meter_text_id_t id,
                    const lv_font_t *font, uint32_t color)
{
    (void)u;
    lv_obj_t *label = meter_text(parent, x, y, meter_i18n_text(id), font, color);
    meter_i18n_bind_label(label, id);
    return label;
}
lv_obj_t *demo_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x142a38), 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 14, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_scrollable(p, false);
    return p;
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
    lv_obj_set_size(u->root, 800, 480);
    lv_obj_set_style_bg_color(u->root, lv_color_hex(0x091a25), 0);
    lv_obj_set_style_bg_opa(u->root, 255, 0);
    lv_obj_set_scrollable(u->root, false);
    demo_text(u, u->root, 22, 15, METER_TXT_FIELD, &lv_font_montserrat_24,
               0x5de5ca);
    demo_text(u, u->root, 111, 20, METER_TXT_REFERENCE,
               &lv_font_montserrat_12, 0x9cb5c4);
    u->connection = demo_text(u, u->root, 491, 18, METER_TXT_WAITING,
                               &lv_font_montserrat_12, 0xf3ba65);
    u->clock = meter_text(u->root, 714, 18, "00:00", &lv_font_montserrat_14, 0xe9f2f5);
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
    {
        u->pages[i] = lv_obj_create(u->root);
        lv_obj_remove_style_all(u->pages[i]);
        lv_obj_set_pos(u->pages[i], 0, 53);
        lv_obj_set_size(u->pages[i], 800, 372);
        lv_obj_set_scrollable(u->pages[i], false);
    }
    demo_dashboard_create(u);
    demo_monitor_create(u);
    demo_faults_create(u);
    demo_settings_create(u);
    demo_navigation_create(u);
    demo_navigation_show(u, 0);
    return u;
}
void demo_ui_present(void *context, const meter_snapshot_t *snapshot, uint32_t elapsed)
{
    demo_ui_t *u = context;
    (void)elapsed;
    u->snapshot = *snapshot;
    if (!u->language_presented || u->presented_language != snapshot->language)
    {
        lv_translation_set_language(snapshot->language == METER_LANGUAGE_ZH ? "zh-CN" : "en");
        u->language_presented = true;
        u->presented_language = snapshot->language;
    }
    bool stale = false;
    for (unsigned i = 0; i < METER_SIGNAL_COUNT; ++i)
        if (snapshot->signals[i].state == METER_VALUE_STALE)
            stale = true;
    strcpy(u->connection_text, !snapshot->connected ? meter_i18n_text(METER_TXT_OFFLINE)
                               : stale              ? meter_i18n_text(METER_TXT_STALE)
                               : snapshot->signals[METER_SPEED].state == METER_VALUE_UNKNOWN
                                   ? meter_i18n_text(METER_TXT_WAITING)
                                   : meter_i18n_text(METER_TXT_CONNECTED));
    lv_label_set_text_static(u->connection, u->connection_text);
    meter_i18n_apply_font(u->connection, snapshot->language);
    uint32_t sec = lv_tick_get() / 1000;
    lv_snprintf(u->clock_text, sizeof(u->clock_text), "%02u:%02u", (unsigned)(sec / 60 % 60),
                (unsigned)(sec % 60));
    lv_label_set_text_static(u->clock, u->clock_text);
    demo_dashboard_update(u);
    demo_monitor_update(u);
    demo_faults_update(u);
    demo_settings_update(u);
    lv_obj_set_style_opa(u->pages[u->page], (lv_opa_t)(80 + snapshot->brightness * 175 / 100), 0);
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
