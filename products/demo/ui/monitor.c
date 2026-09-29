#include "ui/common/formatter/meter_format.h"
#include "ui/demo_internal.h"
void demo_monitor_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_MONITOR];
    demo_text(u, p, 24, 4, DEMO_TXT_LIVE_TELEMETRY, &lv_font_montserrat_24, 0xedf5f8);
    demo_text(u, p, 24, 33, DEMO_TXT_TELEMETRY_SUBTITLE, &lv_font_montserrat_16, 0x8ba9bb);
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        int col = (int)(i / 7), row = (int)(i % 7);
        lv_obj_t *r = demo_panel(p, 16 + col * 390, 61 + row * 44, 378, 40);
        u->monitor_labels[i] =
            meter_text(r, 10, 10, demo_i18n_monitor_label(i), &lv_font_montserrat_16, 0x9cb5c4);
        u->monitor_values[i] = meter_text(r, 182, 10, "--", &lv_font_montserrat_16, 0x5de5ca);
    }
}
void demo_monitor_update(demo_ui_t *u)
{
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        const demo_monitor_view_t *d = &u->view.monitors[i];
        demo_readout_t v = d->reading;
        lv_label_set_text(u->monitor_labels[i], demo_i18n_monitor_label(i));
        meter_i18n_apply_font(u->monitor_labels[i], u->view.language, METER_FONT_LABEL);
        meter_i18n_format_value(u->monitor_text[i], sizeof(u->monitor_text[i]), v.value, v.state, d->unit, 1,
                                u->view.language);
        meter_i18n_apply_font(u->monitor_values[i], u->view.language, METER_FONT_LABEL);
        lv_label_set_text_static(u->monitor_values[i], u->monitor_text[i]);
    }
}
