#include "ui/products/demo/demo_internal.h"
static void navigate(lv_event_t *e)
{
    demo_ui_t *u = lv_event_get_user_data(e);
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
        if (lv_event_get_target_obj(e) == u->nav[i])
            demo_navigation_show(u, i);
}
void demo_navigation_show(demo_ui_t *u, unsigned page)
{
    if (page >= DEMO_PAGE_COUNT)
        return;
    u->page = page;
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
    {
        if (i == page)
            lv_obj_set_hidden(u->pages[i], false);
        else
            lv_obj_set_hidden(u->pages[i], true);
        lv_obj_set_style_bg_color(u->nav[i], lv_color_hex(i == page ? 0x245b50 : 0x142a38), 0);
    }
}
void demo_navigation_create(demo_ui_t *u)
{
    const meter_text_id_t ids[] = {METER_TXT_DASHBOARD, METER_TXT_MONITOR, METER_TXT_FAULTS,
                                   METER_TXT_SETTINGS};
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
    {
        u->nav[i] = lv_button_create(u->root);
        demo_theme_button(u->nav[i]);
        lv_obj_set_pos(u->nav[i], 16 + (int)i * 195, 431);
        lv_obj_set_size(u->nav[i], 183, 37);
        lv_obj_set_style_shadow_width(u->nav[i], 0, 0);
        lv_obj_set_style_radius(u->nav[i], 8, 0);
        u->nav_labels[i] = demo_text(u, u->nav[i], 0, 0, ids[i],
                                      &lv_font_montserrat_14, 0xe8f1f4);
        lv_obj_center(u->nav_labels[i]);
        lv_obj_add_event_cb(u->nav[i], navigate, LV_EVENT_CLICKED, u);
    }
}

