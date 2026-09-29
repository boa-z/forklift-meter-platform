#include "core/meter_core.h"
#include "platform/host/host_platform.h"
#include "ui/demo_internal.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static unsigned actions_sent;
static bool accept = true;
static bool send(void *context, const meter_action_t *action)
{
    ++actions_sent;
    return accept && meter_core_action(context, action);
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
                    (int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2,
                    (int)bounds.x1, (int)bounds.y1, (int)bounds.x2, (int)bounds.y2);
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
        CHECK(lv_obj_is_hidden(ui->settings_cards[0]) == (subpage != 0));
        CHECK(lv_obj_is_hidden(ui->settings_cards[1]) == (subpage != 1));
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
                CHECK(name.y2 < value.y1);
            }
        }
        else
        {
            CHECK(!strcmp(lv_label_get_text(label), ui->fault_text[i]));
            CHECK(strstr(lv_label_get_text(label), demo_i18n_fault_description(i)));
            CHECK(strstr(lv_label_get_text(label), demo_i18n_text(ui->view.faults[i].active ? DEMO_TXT_ACTIVE : DEMO_TXT_CLEAR)));
        }
    }
    return 0;
}
int main(int argc, char **argv)
{
    meter_core_t core;
    meter_value_t signals[DEMO_SIGNAL_SLOTS];
    float parameters[DEMO_PARAMETER_SLOTS];
    meter_fault_state_t faults[DEMO_FAULT_SLOTS];
    meter_core_storage_t storage = {signals, DEMO_SIGNAL_SLOTS, parameters, DEMO_PARAMETER_SLOTS, faults, DEMO_FAULT_SLOTS};
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
            for (unsigned page = DEMO_MONITOR; page <= DEMO_SETTINGS; ++page)
            {
                demo_pager_t *pager = pagers[page - DEMO_MONITOR];
                CHECK(pager->count == 2);
                demo_navigation_show(ui, page);
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
                    CHECK(!strcmp(lv_label_get_text(pager->indicator), subpage ? "2 / 2" : "1 / 2"));
                    CHECK(lv_obj_has_state(pager->next, LV_STATE_DISABLED) == (subpage == 1));
                    CHECK(check_rows(ui, page, subpage) == 0);
                    CHECK(check_layout(ui->pages[page]) == 0);
                    CHECK(meter_ui_object_count(ui->root) == objects);
                    CHECK(actions_sent == 0);
                    if (argc == 2 && round < 2 && state == METER_VALUE_VALID)
                    {
                        char path[1024];
                        snprintf(path, sizeof(path), "%s/page-%u-%u-%s.bmp", argv[1], page, subpage + 1, round ? "zh" : "en");
                        CHECK(meter_host_capture(path));
                    }
                }
                lv_obj_send_event(pager->next, LV_EVENT_CLICKED, NULL);
                CHECK(pager->current == 1);
                demo_monitor_show_page(ui, UINT_MAX);
                demo_faults_show_page(ui, UINT_MAX);
                demo_settings_show_page(ui, UINT_MAX);
                CHECK(pager->current == 1);
                lv_obj_send_event(pager->previous, LV_EVENT_CLICKED, NULL);
                CHECK(pager->current == 0);
            }
        }
    }
    /* 翻页后的设置仍通过既有动作入口，拒绝状态不能因翻页消失。 */
    demo_settings_show_page(ui, 1);
    lv_slider_set_value(ui->brightness, 70, LV_ANIM_OFF);
    lv_obj_send_event(ui->brightness, LV_EVENT_RELEASED, NULL);
    CHECK(core.snapshot.brightness == 70 && actions_sent == 1);
    accept = false;
    lv_slider_set_value(ui->brightness, 80, LV_ANIM_OFF);
    lv_obj_send_event(ui->brightness, LV_EVENT_RELEASED, NULL);
    demo_settings_show_page(ui, 0);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(core.snapshot.brightness == 70 && ui->action_failed && actions_sent == 2);
    CHECK(!strcmp(lv_label_get_text(ui->setting_status), demo_i18n_text(DEMO_TXT_SETTINGS_ERROR)));
    demo_ui_destroy(ui);
    meter_host_close();
    lv_deinit();
    puts("Pagination, bilingual bounds, retained readings, end stops and settings PASS");
    return 0;
}
