#include "core/meter_core.h"
#include "platform/host/host_platform.h"
#include "services/settings_app.h"
#include "ui/demo_internal.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do                                                                                                       \
    {                                                                                                        \
        if (!(condition))                                                                                    \
        {                                                                                                    \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                                          \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

static unsigned actions_sent;
static bool accept = true;
static bool send(void *context, const meter_action_t *action)
{
    ++actions_sent;
    if (!accept || !demo_settings_action(action, lv_tick_get()))
        return false;
    return action->kind == METER_ACTION_PRODUCT || meter_core_action(context, action);
}

/* 对实际布局求边界；仅验证可见内容，不能用隐藏溢出来掩盖排版问题。 */
static int check_layout(lv_obj_t *parent)
{
    lv_area_t bounds;
    lv_obj_get_coords(parent, &bounds);
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
    {
        lv_obj_t *child = lv_obj_get_child(parent, (int32_t)i);
        if (lv_obj_is_hidden(child))
            continue;
        lv_area_t area;
        lv_obj_get_coords(child, &area);
        if (area.x1 < bounds.x1 || area.y1 < bounds.y1 || area.x2 > bounds.x2 || area.y2 > bounds.y2)
        {
            fprintf(stderr, "overflow %s: (%d,%d)-(%d,%d) in (%d,%d)-(%d,%d)\n",
                    lv_obj_check_type(child, &lv_label_class) ? lv_label_get_text(child) : "widget",
                    (int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2, (int)bounds.x1, (int)bounds.y1,
                    (int)bounds.x2, (int)bounds.y2);
            return 1;
        }
        CHECK(check_layout(child) == 0);
    }
    return 0;
}
static int check_rows(demo_ui_t *ui, unsigned page, unsigned subpage)
{
    size_t slots = page == DEMO_MONITOR ? DEMO_MONITOR_SLOTS : DEMO_FAULT_SLOTS;
    unsigned capacity = page == DEMO_MONITOR ? DEMO_MONITORS_PER_PAGE : DEMO_FAULTS_PER_PAGE;
    if (page == DEMO_SETTINGS)
    {
        for (unsigned i = 0; i < 4; ++i)
            CHECK(lv_obj_is_hidden(ui->settings_cards[i]) == (subpage != i));
        return 0;
    }
    for (size_t i = 0; i < slots; ++i)
    {
        lv_obj_t *label = page == DEMO_MONITOR ? ui->monitor_labels[i] : ui->fault_rows[i];
        lv_obj_t *row = lv_obj_get_parent(label);
        CHECK(lv_obj_is_hidden(row) == (i / capacity != subpage));
        if (page == DEMO_MONITOR)
        {
            CHECK(!strcmp(lv_label_get_text(ui->monitor_values[i]), ui->monitor_text[i]));
            if (i / capacity == subpage)
            {
                lv_area_t name, value;
                lv_obj_get_coords(label, &name);
                lv_obj_get_coords(ui->monitor_values[i], &value);
                CHECK(name.x2 < value.x1);
            }
        }
        else
        {
            CHECK(!strcmp(lv_label_get_text(label), ui->fault_text[i]));
            CHECK(strstr(lv_label_get_text(label), demo_i18n_fault_description(i)));
            CHECK(strstr(lv_label_get_text(label),
                         demo_i18n_text(ui->view.faults[i].active ? DEMO_TXT_ACTIVE : DEMO_TXT_CLEAR)));
        }
    }
    return 0;
}
/* 检查连续触摸区和双语布局，防止全宽导航留下空隙或裁掉标签。 */
static int check_navigation(demo_ui_t *ui)
{
    for (unsigned i = 0; i < DEMO_PAGE_COUNT; ++i)
    {
        lv_area_t area;
        lv_obj_get_coords(ui->nav[i], &area);
        CHECK(area.x1 == (int)i * 200 && area.x2 == (int)(i + 1) * 200 - 1);
        CHECK(area.y1 == 425 && area.y2 == 479);
        CHECK(check_layout(ui->nav[i]) == 0);
        lv_obj_send_event(ui->nav[i], LV_EVENT_CLICKED, NULL);
        CHECK(ui->page == i);
    }
    return 0;
}
static int check_dashboard_footer(demo_ui_t *ui)
{
    unsigned icons = 0, readings = 0;
    lv_obj_t *page = ui->pages[DEMO_DASHBOARD];
    for (uint32_t i = 0; i < lv_obj_get_child_count(page); ++i)
    {
        lv_obj_t *panel = lv_obj_get_child(page, (int32_t)i);
        if (lv_obj_get_y(panel) != 314)
            continue;
        CHECK(check_layout(panel) == 0);
        for (uint32_t j = 0; j < lv_obj_get_child_count(panel); ++j)
        {
            lv_obj_t *child = lv_obj_get_child(panel, (int32_t)j);
            if (lv_obj_check_type(child, &lv_label_class))
            {
                ++readings;
                continue;
            }
            for (uint32_t k = 0; k < lv_obj_get_child_count(child); ++k)
            {
                lv_obj_t *element = lv_obj_get_child(child, (int32_t)k);
                if (lv_obj_check_type(element, &lv_label_class))
                    CHECK(lv_obj_is_hidden(element) && strlen(lv_label_get_text(element)) == 0);
                if (lv_obj_check_type(element, &lv_image_class))
                {
                    CHECK(!lv_obj_is_hidden(element));
                    CHECK(lv_image_get_scale_x(element) == 512 && lv_image_get_scale_y(element) == 512);
                    ++icons;
                }
            }
        }
    }
    CHECK(icons == 5 && readings == 4);
    return 0;
}
/* 监控页与设置页共用左侧分类栏、右侧条目面板的几何约束。 */
static int check_monitor_layout(demo_ui_t *ui)
{
    lv_area_t first_tab, second_tab, first_row;
    lv_obj_get_coords(ui->monitor_mode_button, &first_tab);
    lv_obj_get_coords(ui->parameter_mode_button, &second_tab);
    lv_obj_get_coords(lv_obj_get_parent(ui->monitor_labels[0]), &first_row);
    CHECK(first_tab.x1 == 24 && first_tab.x2 == 191 && first_tab.y1 == 123 && first_tab.y2 == 174);
    CHECK(second_tab.x1 == 24 && second_tab.x2 == 191 && second_tab.y1 == 185 && second_tab.y2 == 236);
    CHECK(first_row.x1 == 216 && first_row.x2 == 783 && first_row.y1 == 123);
    CHECK(first_row.y2 < 365);
    return 0;
}
int main(int argc, char **argv)
{
    meter_core_t core;
    meter_value_t signals[DEMO_SIGNAL_SLOTS];
    float parameters[DEMO_PARAMETER_SLOTS];
    meter_fault_state_t faults[DEMO_FAULT_SLOTS];
    meter_core_storage_t storage = {signals, DEMO_SIGNAL_SLOTS, parameters, DEMO_PARAMETER_SLOTS,
                                    faults,  DEMO_FAULT_SLOTS};
    CHECK(meter_core_init(&core, &meter_demo_catalog, &storage));
    lv_init();
    CHECK(meter_i18n_init() && demo_i18n_init() && meter_host_open(true));
    meter_ui_actions_t actions = {send, &core};
    demo_ui_t *ui = demo_ui_create(lv_screen_active(), &actions);
    CHECK(ui);
    size_t objects = meter_ui_object_count(ui->root);
    demo_pager_t *pagers[] = {&ui->monitor_pager, &ui->fault_pager, &ui->settings_pager};
    for (unsigned round = 0; round < 3; ++round)
    {
        core.snapshot.language = round == 1 ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN;
        core.snapshot.brightness = 100;
        for (unsigned state = METER_VALUE_UNKNOWN; state <= METER_VALUE_ERROR; ++state)
        {
            for (size_t i = 0; i < core.snapshot.catalog->signal_count; ++i)
            {
                core.snapshot.signals[i].state = (meter_value_state_t)state;
                core.snapshot.signals[i].value = state == METER_VALUE_VALID ? 0 : -123.4f;
            }
            for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
                core.snapshot.faults[i].active = (i + state) % 2 != 0;
            demo_ui_present(ui, &core.snapshot, 16);
            lv_obj_update_layout(ui->root);
            CHECK(check_navigation(ui) == 0);
            demo_navigation_show(ui, DEMO_DASHBOARD);
            CHECK(check_dashboard_footer(ui) == 0);
            demo_navigation_show(ui, DEMO_MONITOR);
            CHECK(check_monitor_layout(ui) == 0);
            for (unsigned page = DEMO_MONITOR; page <= DEMO_SETTINGS; ++page)
            {
                demo_pager_t *pager = pagers[page - DEMO_MONITOR];
                unsigned expected_count = page == DEMO_SETTINGS
                    ? 4u
                    : page == DEMO_MONITOR
                        ? (DEMO_MONITOR_SLOTS + DEMO_MONITORS_PER_PAGE - 1) / DEMO_MONITORS_PER_PAGE
                        : (DEMO_FAULT_SLOTS + DEMO_FAULTS_PER_PAGE - 1) / DEMO_FAULTS_PER_PAGE;
                CHECK(pager->count == expected_count);
                demo_navigation_show(ui, page);
                if (page == DEMO_MONITOR)
                    demo_monitor_show_page(ui, 0);
                else if (page == DEMO_FAULTS)
                    demo_faults_show_page(ui, 0);
                else
                    demo_settings_show_page(ui, 0);
                lv_obj_send_event(pager->previous, LV_EVENT_CLICKED, NULL);
                CHECK(pager->current == 0 && lv_obj_has_state(pager->previous, LV_STATE_DISABLED));
                for (unsigned subpage = 0; subpage < pager->count; ++subpage)
                {
                    if (subpage)
                        lv_obj_send_event(pager->next, LV_EVENT_CLICKED, NULL);
                    /* 实时更新和切换主页面必须保留当前子页，且不覆盖页码或读数。 */
                    demo_navigation_show(ui, DEMO_DASHBOARD);
                    demo_navigation_show(ui, page);
                    demo_ui_present(ui, &core.snapshot, 16);
                    lv_obj_update_layout(ui->root);
                    lv_tick_inc(32);
                    lv_timer_handler();
                    CHECK(pager->current == subpage);
                    char expected_page[16];
                    snprintf(expected_page, sizeof(expected_page), "%u/%u", subpage + 1u, pager->count);
                    CHECK(!strcmp(lv_label_get_text(pager->indicator), expected_page));
                    CHECK(lv_obj_has_state(pager->next, LV_STATE_DISABLED) == (subpage + 1u == pager->count));
                    CHECK(check_rows(ui, page, subpage) == 0);
                    CHECK(check_layout(ui->pages[page]) == 0);
                    CHECK(meter_ui_object_count(ui->root) == objects);
                    CHECK(actions_sent == 0);
                    if (argc == 2 && round < 2 && state == METER_VALUE_VALID)
                    {
                        char path[1024];
                        snprintf(path, sizeof(path), "%s/page-%u-%u-%s.bmp", argv[1], page, subpage + 1,
                                 round ? "zh" : "en");
                        CHECK(meter_host_capture(path));
                    }
                }
                lv_obj_send_event(pager->next, LV_EVENT_CLICKED, NULL);
                CHECK(pager->current == pager->count - 1u);
                demo_monitor_show_page(ui, UINT_MAX);
                demo_faults_show_page(ui, UINT_MAX);
                demo_settings_show_page(ui, UINT_MAX);
                CHECK(pager->current == pager->count - 1u);
                lv_obj_send_event(pager->previous, LV_EVENT_CLICKED, NULL);
                CHECK(pager->current == (pager->count > 1u ? pager->count - 2u : 0u));
            }
        }
    }
    /* 翻页后的设置仍通过既有动作入口，拒绝状态不能因翻页消失。 */
    demo_settings_show_page(ui, 1);
    lv_obj_send_event(ui->admin_password_button, LV_EVENT_CLICKED, NULL);
    if (argc == 2)
    {
        char password_capture[1024];
        snprintf(password_capture, sizeof(password_capture), "%s/password-page.bmp", argv[1]);
        CHECK(meter_host_capture(password_capture));
    }
    lv_textarea_set_text(ui->admin_password, "0000");
    lv_obj_send_event(ui->password_keyboard, LV_EVENT_READY, NULL);
    demo_settings_publish(&core.snapshot);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(!ui->view.admin_authorized && lv_obj_is_hidden(ui->admin_items[0]));
    lv_obj_send_event(ui->admin_password_button, LV_EVENT_CLICKED, NULL);
    lv_textarea_set_text(ui->admin_password, "5312");
    lv_obj_send_event(ui->password_keyboard, LV_EVENT_READY, NULL);
    demo_settings_publish(&core.snapshot);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(ui->view.admin_authorized && !lv_obj_is_hidden(ui->admin_items[0]));
    CHECK(strlen(lv_textarea_get_text(ui->admin_password)) == 0);
    demo_navigation_show(ui, DEMO_MONITOR);
    lv_obj_send_event(ui->parameter_mode_button, LV_EVENT_CLICKED, NULL);
    CHECK(ui->monitor_pager.count == 1);
    lv_obj_send_event(ui->parameter_rows[0], LV_EVENT_CLICKED, NULL);
    CHECK(!lv_obj_is_hidden(ui->parameter_editor));
    lv_textarea_set_text(ui->parameter_input, "");
    lv_obj_send_event(ui->parameter_keyboard, LV_EVENT_READY, NULL);
    CHECK(actions_sent == 2);
    lv_obj_send_event(ui->parameter_rows[0], LV_EVENT_CLICKED, NULL);
    lv_textarea_set_text(ui->parameter_input, "42.0");
    lv_obj_send_event(ui->parameter_keyboard, LV_EVENT_READY, NULL);
    demo_settings_run(lv_tick_get(), NULL);
    demo_settings_publish(&core.snapshot);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(core.snapshot.parameters[0] == 25.0f && actions_sent == 3);
    CHECK(ui->view.parameters[0].value == 42.0f && ui->view.parameter_feedback == DEMO_FEEDBACK_APPLIED);
    lv_slider_set_value(ui->brightness, 70, LV_ANIM_OFF);
    lv_obj_send_event(ui->brightness, LV_EVENT_RELEASED, NULL);
    CHECK(core.snapshot.brightness == 70 && actions_sent == 4);
    accept = false;
    lv_slider_set_value(ui->brightness, 80, LV_ANIM_OFF);
    lv_obj_send_event(ui->brightness, LV_EVENT_RELEASED, NULL);
    demo_settings_show_page(ui, 0);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(core.snapshot.brightness == 70 && ui->action_failed && actions_sent == 5);
    CHECK(!strcmp(lv_label_get_text(ui->setting_status), demo_i18n_text(DEMO_TXT_ACCESS_DENIED)));
    demo_ui_destroy(ui);
    meter_host_close();
    lv_deinit();
    puts("Pagination, bilingual bounds, retained readings, end stops and settings PASS");
    return 0;
}
