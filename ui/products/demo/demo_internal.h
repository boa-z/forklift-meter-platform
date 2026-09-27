#ifndef DEMO_INTERNAL_H
#define DEMO_INTERNAL_H
#include "generated/demo_catalog.h"
#include "generated/demo_icons.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/products/demo/demo_ui.h"
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
    lv_obj_t *root, *pages[DEMO_PAGE_COUNT], *nav[DEMO_PAGE_COUNT], *connection, *clock;
    meter_ui_actions_t actions;
    unsigned page;
    meter_snapshot_t snapshot;
    meter_gauge_t *speed, *steering;
    meter_ring_t *soc, *load_arc;
    meter_linear_meter_t *height;
    meter_value_label_t *load, *hours;
    meter_status_t *status[5];
    lv_obj_t *monitor_values[METER_MONITOR_CAPACITY], *fault_rows[METER_FAULT_CAPACITY];
    lv_obj_t *unit_button, *brightness, *limit, *setting_status;
    char monitor_text[METER_MONITOR_CAPACITY][64], fault_text[METER_FAULT_CAPACITY][100];
    char clock_text[32], connection_text[48];
    bool action_failed;
} demo_ui_t;
lv_obj_t *demo_panel(lv_obj_t *parent, int x, int y, int width, int height);
void demo_dashboard_create(demo_ui_t *ui);
void demo_dashboard_update(demo_ui_t *ui, uint32_t elapsed_ms);
void demo_monitor_create(demo_ui_t *ui);
void demo_monitor_update(demo_ui_t *ui);
void demo_faults_create(demo_ui_t *ui);
void demo_faults_update(demo_ui_t *ui);
void demo_settings_create(demo_ui_t *ui);
void demo_settings_update(demo_ui_t *ui);
void demo_navigation_create(demo_ui_t *ui);
void demo_navigation_show(demo_ui_t *ui, unsigned page);
#endif
