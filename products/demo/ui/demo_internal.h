#ifndef DEMO_INTERNAL_H
#define DEMO_INTERNAL_H
#include "application/presentation.h"
#include "generated/demo_icons.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/common/widgets/update/meter_update_widget.h"
#include "ui/demo_i18n.h"
#include "ui/demo_theme.h"
#include "ui/demo_ui.h"
enum
{
    DEMO_DASHBOARD,
    DEMO_MONITOR,
    DEMO_FAULTS,
    DEMO_SETTINGS,
    DEMO_PAGE_COUNT
};
typedef struct
{
    lv_obj_t *root, *pages[DEMO_PAGE_COUNT], *nav[DEMO_PAGE_COUNT], *nav_labels[DEMO_PAGE_COUNT], *connection,
        *clock;
    meter_update_widget_t update_widget;
    meter_ui_actions_t actions;
    unsigned page;
    demo_presentation_t view;
    meter_gauge_t *speed, *steering;
    meter_ring_t *soc, *load_arc;
    meter_linear_meter_t *height;
    meter_value_label_t *load, *hours;
    meter_status_t *status[5];
    lv_obj_t *monitor_labels[DEMO_MONITOR_SLOTS], *monitor_values[DEMO_MONITOR_SLOTS],
        *fault_rows[DEMO_FAULT_SLOTS];
    lv_obj_t *unit_button, *language_button, *brightness, *limit, *setting_status;
    char monitor_text[DEMO_MONITOR_SLOTS][64], fault_text[DEMO_FAULT_SLOTS][100];
    char clock_text[32], connection_text[48];
    bool action_failed;
    bool language_presented;
    meter_language_t presented_language;
} demo_ui_t;
lv_obj_t *demo_text(demo_ui_t *ui, lv_obj_t *parent, int x, int y, demo_text_id_t id, const lv_font_t *font,
                    uint32_t color);
lv_obj_t *demo_panel(lv_obj_t *parent, int x, int y, int width, int height);
void demo_dashboard_create(demo_ui_t *ui);
void demo_dashboard_update(demo_ui_t *ui);
void demo_monitor_create(demo_ui_t *ui);
void demo_monitor_update(demo_ui_t *ui);
void demo_faults_create(demo_ui_t *ui);
void demo_faults_update(demo_ui_t *ui);
void demo_settings_create(demo_ui_t *ui);
void demo_settings_update(demo_ui_t *ui);
void demo_navigation_create(demo_ui_t *ui);
void demo_navigation_show(demo_ui_t *ui, unsigned page);
#endif
