#include "ui/common/formatter/meter_format.h"
#include "ui/products/demo/demo_internal.h"
void demo_monitor_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_MONITOR];
    demo_text(u, p, 24, 4, METER_TXT_LIVE_TELEMETRY, &lv_font_montserrat_20,
               0xedf5f8);
    demo_text(u, p, 24, 33, METER_TXT_TELEMETRY_SUBTITLE,
               &lv_font_montserrat_12, 0x8ba9bb);
    for (size_t i = 0; i < meter_demo_catalog.monitor_count; ++i)
    {
        int col = (int)(i / 7), row = (int)(i % 7);
        lv_obj_t *r = demo_panel(p, 16 + col * 390, 61 + row * 42, 378, 37);
        u->monitor_labels[i] = meter_text(r, 10, 10, meter_i18n_monitor_label(i),
                                          &lv_font_montserrat_12, 0x9cb5c4);
        u->monitor_values[i] = meter_text(r, 182, 10, "--", &lv_font_montserrat_12, 0x5de5ca);
    }
}
void demo_monitor_update(demo_ui_t *u)
{
    for (size_t i = 0; i < meter_demo_catalog.monitor_count; ++i)
    {
        const meter_monitor_def_t *d = &meter_demo_catalog.monitors[i];
        lv_label_set_text(u->monitor_labels[i], meter_i18n_monitor_label(i));
        meter_i18n_apply_font(u->monitor_labels[i], u->snapshot.language);
        const meter_value_t *v = &u->snapshot.signals[d->signal];
        meter_i18n_format_value(u->monitor_text[i], sizeof(u->monitor_text[i]), v->value, v->state, d->unit, 1, u->snapshot.language);
        meter_i18n_apply_font(u->monitor_values[i], u->snapshot.language);
        lv_label_set_text_static(u->monitor_values[i], u->monitor_text[i]);
    }
}
