#include "ui/ui.h"
#include "catalog/catalog.h"
#include <lvgl.h>
static const char *const languages[] = {"en", "zh-CN", NULL};
static const char *const tags[] = {"mix.title", "mix.can0", "mix.can1", "mix.sync", NULL};
static const char *const translations[] = {"Mixed", "混合", "CAN0 DBC", "CAN0 私有",
                                           "CAN1 PDO/SDO", "CAN1 过程/服务",
                                           "Sync", "同步"};
typedef struct
{
    lv_obj_t *tabs;
    lv_obj_t *values[4];
    meter_language_t language;
} view_t;
static view_t view;
static void *create(void *parent, const meter_ui_actions_t *actions)
{
    (void)actions;
    if (!lv_translation_add_static(languages, tags, translations))
        return NULL;
    view.tabs = lv_tabview_create(parent);
    view.language = (meter_language_t)255;
    lv_obj_set_size(view.tabs, 800, 480);
    lv_tabview_set_tab_bar_size(view.tabs, 64);
    for (unsigned page = 0; page < 2; ++page)
    {
        lv_obj_t *tab = lv_tabview_add_tab(view.tabs, page ? "CAN1" : "CAN0");
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        for (unsigned row = 0; row < 2; ++row)
        {
            view.values[page * 2 + row] = lv_label_create(tab);
            lv_obj_set_width(view.values[page * 2 + row], 680);
        }
    }
    return &view;
}
static void present(void *ui, const meter_snapshot_t *s, uint32_t elapsed)
{
    (void)elapsed;
    view_t *v = ui;
    if (v->language != s->language)
    {
        v->language = s->language;
        lv_translation_set_language(s->language == METER_LANGUAGE_ZH ? "zh-CN" : "en");
    }
    const meter_signal_id_t ids[] = {MIXED_CAN0_SPEED, MIXED_CAN0_SOC, MIXED_PDO_SPEED, MIXED_PDO_TORQUE};
    for (unsigned i = 0; i < 4; ++i)
    {
        meter_value_t value = meter_snapshot_read(s, ids[i]);
        lv_label_set_text_fmt(v->values[i], "%u: %.1f [%u]", ids[i], (double)value.value,
                              (unsigned)value.state);
    }
}
static void destroy(void *ui)
{
    lv_obj_delete(((view_t *)ui)->tabs);
}
const meter_ui_factory_t product_ui = {.create = create, .present = present, .destroy = destroy};
