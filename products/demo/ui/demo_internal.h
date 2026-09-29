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
enum
{
    DEMO_LIST_CAPACITY = 5,
    DEMO_LIST_TOP = 58,
    DEMO_LIST_PITCH = 60,
    DEMO_LIST_HEIGHT = 56,
    DEMO_MONITORS_PER_PAGE = DEMO_LIST_CAPACITY,
    DEMO_FAULTS_PER_PAGE = DEMO_LIST_CAPACITY
};
/* 翻页只属于 Demo 展示层，不改变快照内容或向 App 发送动作。 */
typedef struct
{
    lv_obj_t *previous, *next, *indicator;
    unsigned current, count;
} demo_pager_t;
typedef struct
{
    lv_obj_t *root, *pages[DEMO_PAGE_COUNT], *nav[DEMO_PAGE_COUNT], *nav_labels[DEMO_PAGE_COUNT],
        *nav_icons[DEMO_PAGE_COUNT], *connection, *clock;
    meter_update_widget_t update_widget;
    meter_ui_actions_t actions;
    unsigned page;
    demo_presentation_t view;
    meter_gauge_t *speed, *steering;
    meter_ring_t *soc, *load_arc;
    meter_linear_meter_t *height;
    meter_value_label_t *load, *mileage, *hours;
    meter_status_t *status[5];
    lv_obj_t *monitor_labels[DEMO_MONITOR_SLOTS], *monitor_values[DEMO_MONITOR_SLOTS],
        *fault_rows[DEMO_FAULT_SLOTS];
    lv_obj_t *parameter_rows[DEMO_REMOTE_COUNT], *parameter_labels[DEMO_REMOTE_COUNT],
        *parameter_values[DEMO_REMOTE_COUNT];
    lv_obj_t *parameter_editor, *parameter_editor_title, *parameter_input, *parameter_keyboard,
        *parameter_status, *monitor_mode_button, *parameter_mode_button;
    lv_obj_t *monitor_mode_label, *parameter_mode_label;
    bool parameter_mode;
    unsigned selected_parameter;
    lv_obj_t *unit_button, *language_button, *brightness, *limit, *setting_status;
    lv_obj_t *settings_title, *settings_note, *settings_rail, *settings_menu[4], *settings_cards[4],
        *version_labels[4], *version_values[4], *version_entry, *version_back, *settings_detail,
        *settings_detail_title, *settings_detail_back, *settings_detail_value, *admin_detail_button;
    lv_obj_t *setting_entries[4], *setting_values[4];
    unsigned settings_tab, selected_setting;
    bool version_open, admin_navigation_pending;
    lv_obj_t *user_password, *admin_password, *user_password_button, *admin_password_button, *password_editor,
        *password_editor_title, *password_keyboard, *password_status, *admin_items[4], *admin_value_labels[4],
        *admin_locked, *logout_button;
    bool password_admin;
    demo_pager_t monitor_pager, fault_pager, settings_pager;
    char monitor_text[DEMO_MONITOR_SLOTS][64], fault_text[DEMO_FAULT_SLOTS][100];
    char parameter_text[DEMO_REMOTE_COUNT][32], parameter_input_text[32];
    char clock_text[32], connection_text[48];
    bool action_failed;
    bool language_presented;
    meter_language_t presented_language;
} demo_ui_t;
lv_obj_t *demo_text(demo_ui_t *ui, lv_obj_t *parent, int x, int y, demo_text_id_t id, const lv_font_t *font,
                    uint32_t color);
lv_obj_t *demo_panel(lv_obj_t *parent, int x, int y, int width, int height);
void demo_pager_create(lv_obj_t *parent, demo_pager_t *pager, unsigned count, lv_event_cb_t callback,
                       void *context);
bool demo_pager_select(demo_pager_t *pager, unsigned page);
unsigned demo_pager_target(const demo_pager_t *pager, lv_event_t *event);
void demo_dashboard_create(demo_ui_t *ui);
void demo_dashboard_update(demo_ui_t *ui);
void demo_monitor_create(demo_ui_t *ui);
void demo_monitor_update(demo_ui_t *ui);
void demo_monitor_show_page(demo_ui_t *ui, unsigned page);
void demo_faults_create(demo_ui_t *ui);
void demo_faults_update(demo_ui_t *ui);
void demo_faults_show_page(demo_ui_t *ui, unsigned page);
void demo_editors_close(demo_ui_t *ui);
void demo_settings_create(demo_ui_t *ui);
void demo_settings_update(demo_ui_t *ui);
void demo_settings_show_page(demo_ui_t *ui, unsigned page);
void demo_navigation_create(demo_ui_t *ui);
void demo_navigation_show(demo_ui_t *ui, unsigned page);
#endif
