#include "ui/demo_internal.h"
void demo_faults_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_FAULTS];
    demo_text(u, p, 24, 4, DEMO_TXT_ADVISORIES, &lv_font_montserrat_24, 0xedf5f8);
    demo_text(u, p, 24, 33, DEMO_TXT_FAULT_SUBTITLE, &lv_font_montserrat_16, 0x8ba9bb);
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
    {
        lv_obj_t *r = demo_panel(p, 16, 58 + (int)i * 34, 768, 31);
        u->fault_rows[i] = meter_text(r, 12, 5, "", &lv_font_montserrat_16, 0x9cb5c4);
    }
}
void demo_faults_update(demo_ui_t *u)
{
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
    {
        bool active = u->view.faults[i].active;
        lv_snprintf(u->fault_text[i], sizeof(u->fault_text[i]), "%s   D%02u   %s",
                    demo_i18n_text(active ? DEMO_TXT_ACTIVE : DEMO_TXT_CLEAR),
                    (unsigned)u->view.faults[i].code, demo_i18n_fault_description(i));
        lv_label_set_text_static(u->fault_rows[i], u->fault_text[i]);
        meter_i18n_apply_font(u->fault_rows[i], u->view.language, METER_FONT_LABEL);
        lv_obj_set_style_text_color(u->fault_rows[i], lv_color_hex(active ? 0xffac80 : 0x7895a8), 0);
    }
}
