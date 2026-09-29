#include "ui/demo_internal.h"
void demo_faults_show_page(demo_ui_t *u, unsigned page)
{
    if (!demo_pager_select(&u->fault_pager, page))
        return;
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
        lv_obj_set_hidden(lv_obj_get_parent(u->fault_rows[i]), i / DEMO_FAULTS_PER_PAGE != page);
}
static void turn_page(lv_event_t *event)
{
    demo_ui_t *u = lv_event_get_user_data(event);
    demo_faults_show_page(u, demo_pager_target(&u->fault_pager, event));
}
void demo_faults_create(demo_ui_t *u)
{
    lv_obj_t *p = u->pages[DEMO_FAULTS];
    demo_text(u, p, 24, 4, DEMO_TXT_ADVISORIES, &lv_font_montserrat_24, 0xedf5f8);
    demo_text(u, p, 24, 33, DEMO_TXT_FAULT_SUBTITLE, &lv_font_montserrat_16, 0x8ba9bb);
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
    {
        lv_obj_t *r = demo_panel(p, 16, 65 + (int)(i % DEMO_FAULTS_PER_PAGE) * 47, 768, 42);
        u->fault_rows[i] = meter_text(r, 16, 10, "", &lv_font_montserrat_20, 0x9cb5c4);
    }
    demo_pager_create(p, &u->fault_pager,
                      (DEMO_FAULT_SLOTS + DEMO_FAULTS_PER_PAGE - 1) / DEMO_FAULTS_PER_PAGE,
                      turn_page, u);
    demo_faults_show_page(u, 0);
}
void demo_faults_update(demo_ui_t *u)
{
    const lv_font_t *font = u->view.language == METER_LANGUAGE_EN
                               ? &lv_font_montserrat_20
                               : meter_font_get(u->view.language, METER_FONT_LABEL);
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
    {
        bool active = u->view.faults[i].active;
        lv_snprintf(u->fault_text[i], sizeof(u->fault_text[i]), "%s   D%02u   %s",
                    demo_i18n_text(active ? DEMO_TXT_ACTIVE : DEMO_TXT_CLEAR),
                    (unsigned)u->view.faults[i].code, demo_i18n_fault_description(i));
        lv_label_set_text_static(u->fault_rows[i], u->fault_text[i]);
        lv_obj_set_style_text_font(u->fault_rows[i], font, 0);
        lv_obj_set_style_text_color(u->fault_rows[i], lv_color_hex(active ? 0xffac80 : 0x7895a8), 0);
    }
}
