#include "ui/common/formatter/meter_format.h"
#include "ui/demo_internal.h"
void demo_monitor_show_page(demo_ui_t *u, unsigned page)
{
    if (!demo_pager_select(&u->monitor_pager, page))
        return;
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
        lv_obj_set_hidden(lv_obj_get_parent(u->monitor_labels[i]),
                          i / DEMO_MONITORS_PER_PAGE != page);
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    demo_monitor_show_page(u, demo_pager_target(&u->monitor_pager, event));
}
void demo_monitor_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_MONITOR];
    demo_text(u, p, 24, 4, DEMO_TXT_LIVE_TELEMETRY, &lv_font_montserrat_24, 0xedf5f8);
    demo_text(u, p, 24, 33, DEMO_TXT_TELEMETRY_SUBTITLE, &lv_font_montserrat_16, 0x8ba9bb);
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        unsigned slot = (unsigned)i % DEMO_MONITORS_PER_PAGE;
        int col = (int)(slot / 4), row = (int)(slot % 4);
        lv_obj_t *r = demo_panel(p, 16 + col * 390, 65 + row * 58, 378, 50);
        u->monitor_labels[i] =
            meter_text(r, 12, 2, demo_i18n_monitor_label(i), &lv_font_montserrat_20, 0x9cb5c4);
        u->monitor_values[i] = meter_text(r, 12, 26, "--", &lv_font_montserrat_20, 0x5de5ca);
    }
    demo_pager_create(p, &u->monitor_pager,
                      (DEMO_MONITOR_SLOTS + DEMO_MONITORS_PER_PAGE - 1) / DEMO_MONITORS_PER_PAGE,
                      turn_page, u);
    demo_monitor_show_page(u, 0);
}
void demo_monitor_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN
                               ? &lv_font_montserrat_20
                               : meter_font_get(u->view.language, METER_FONT_LABEL);
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        const demo_monitor_view_t *d = &u->view.monitors[i];
        demo_readout_t v = d->reading;
        lv_label_set_text(u->monitor_labels[i], demo_i18n_monitor_label(i));
        lv_obj_set_style_text_font(u->monitor_labels[i], font, 0);
        meter_i18n_format_value(u->monitor_text[i], sizeof(u->monitor_text[i]), v.value, v.state, d->unit, 1,
                                u->view.language);
        lv_obj_set_style_text_font(u->monitor_values[i], font, 0);
        lv_label_set_text_static(u->monitor_values[i], u->monitor_text[i]);
    }
}
