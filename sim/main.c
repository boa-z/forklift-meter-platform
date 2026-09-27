#include "ui/common/i18n/meter_i18n_runtime.h"
#include "platform/host/host_platform.h"
#include "products/demo/demo_storage.h"
#include "products/demo/product.h"
#include "runtime/meter_runtime.h"
#include "sim/synthetic.h"
#include "sim/domain_fixture.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/products/demo/demo_i18n.h"
#include "ui/products/demo/demo_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct
{
    meter_core_t *core;
    const char *path;
    const meter_product_t *product;
    uint8_t *settings;
    size_t capacity;
} action_context_t;
static bool action(void *context, const meter_action_t *a)
{
    action_context_t *c = context;
    if (!c->product->auth->local_settings ||
        (a->kind == METER_ACTION_PARAMETER && !c->product->capabilities->parameter_write))
        return false;
    /* core 通过指针写产品存储，所以保存失败不能靠丢弃 core 副本来回退，
     * 只能把本次动作可能改到的字段逐项还原。 */
    size_t index = 0;
    float previous = 0;
    bool tracked = false;
    if (a->kind == METER_ACTION_PARAMETER)
    {
        index = meter_catalog_parameter_index(c->core->snapshot.catalog, a->id);
        tracked = index < c->core->snapshot.catalog->parameter_count;
        if (tracked)
            previous = c->core->snapshot.parameters[index];
    }
    bool imperial = c->core->snapshot.imperial;
    meter_language_t language = c->core->snapshot.language;
    uint8_t brightness = c->core->snapshot.brightness;
    if (!meter_core_action(c->core, a))
        return false;
    if (meter_host_save(c->core, c->path, c->settings, c->capacity))
        return true;
    c->core->snapshot.imperial = imperial;
    c->core->snapshot.language = language;
    c->core->snapshot.brightness = brightness;
    if (tracked)
        c->core->snapshot.parameters[index] = previous;
    return false;
}
int main(int argc, char **argv)
{
    unsigned frames = 0, page = 0;
    bool smoke = false, hidden = false;
    const char *capture = NULL, *settings = NULL, *visual = NULL, *set_units = NULL, *set_language = NULL;
    demo_scenario_t scenario = DEMO_NORMAL;
    const char *fixture = NULL;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--smoke"))
        {
            smoke = true;
            hidden = true;
            frames = 240;
        }
        else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
            fixture = argv[++i];
        else if (!strcmp(argv[i], "--hidden"))
            hidden = true;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--settings") && i + 1 < argc)
            settings = argv[++i];
        else if (!strcmp(argv[i], "--visual") && i + 1 < argc)
            visual = argv[++i];
        else if (!strcmp(argv[i], "--set-units") && i + 1 < argc)
            set_units = argv[++i];
        else if (!strcmp(argv[i], "--set-language") && i + 1 < argc)
            set_language = argv[++i];
        else if (!strcmp(argv[i], "--page") && i + 1 < argc)
            page = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--scenario") && i + 1 < argc)
        {
            const char *name = argv[++i];
            const char *names[] = {"normal", "warning", "stale", "offline", "error", "unknown"};
            bool found = false;
            for (unsigned j = 0; j < 6; ++j)
                if (!strcmp(name, names[j]))
                {
                    scenario = (demo_scenario_t)j;
                    found = true;
                }
            if (!found)
            {
                fprintf(stderr, "Unknown scenario\n");
                return 2;
            }
        }
        else
        {
            printf("Usage: meter-demo [--smoke] [--frames N] [--hidden] [--scenario "
                   "normal|warning|stale|offline|error|unknown] [--visual "
                   "min|mid|max] [--capture image.bmp] [--page 0..3] [--settings "
                   "file] [--set-units metric|imperial] [--set-language english|chinese]\n");
            return !strcmp(argv[i], "--help") ? 0 : 2;
        }
    }
    if (page > 3 || (visual && strcmp(visual, "min") && strcmp(visual, "mid") && strcmp(visual, "max")) ||
        (set_units && strcmp(set_units, "metric") && strcmp(set_units, "imperial")) ||
        (set_language && strcmp(set_language, "english") && strcmp(set_language, "chinese")))
        return 2;
    const meter_product_t *product = meter_product_get();
    meter_core_t core;
    meter_runtime_t runtime;
    demo_domain_store_t store;
    meter_core_storage_t storage = demo_domain_bind(&store);
    if (!meter_core_init(&core, product->catalog, &storage) ||
        !meter_runtime_init(&runtime, product, meter_core_apply, &core))
        return 3;
    meter_host_load(&core, settings, store.settings, sizeof(store.settings));
    action_context_t act = {&core, settings, product, store.settings, sizeof(store.settings)};
    meter_ui_actions_t actions = {action, &act};
    if (set_units)
    {
        meter_action_t a = {METER_ACTION_UNITS, 0, !strcmp(set_units, "imperial")};
        if (!action(&act, &a))
            return 4;
    }
    if (set_language)
    {
        meter_action_t a = {METER_ACTION_LANGUAGE, 0,
                            !strcmp(set_language, "chinese") ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN};
        if (!action(&act, &a))
            return 4;
    }
    lv_init();
    if (!meter_i18n_init())
        return 5;
    if (!demo_i18n_init())
        return 5;
    if (!meter_host_open(hidden))
    {
        fprintf(stderr, "SDL display initialization failed\n");
        return 5;
    }
    void *ui = product->ui->create(lv_screen_active(), &actions);
    if (!ui)
        return 6;
    size_t objects = meter_ui_object_count(lv_screen_active()), heap_peak = 0;
    unsigned completed = 0;
    bool navigation_ok = true;
    double max_us = 0, sum_us = 0;
    meter_runtime_connection(&runtime, true);
    if (fixture && !meter_fixture_load(&core, fixture)) return 8;
    if (scenario == DEMO_STALE || scenario == DEMO_OFFLINE)
    {
        meter_synthetic_values(&runtime, 0, 25, 50, -15);
        meter_runtime_poll(&runtime, 32);
    }
    for (unsigned n = 0; (!frames || n < frames); ++n)
    {
        uint32_t now = n * 16;
        if (page && (n == 4 || n == 7))
            meter_host_click(100 + (int)page * 195, 450, n == 4);
        if (smoke)
        {
            if (n == 20 || n == 23)
                meter_host_click(295, 450, n == 20);
            if (n == 30 && demo_ui_active_page(ui) != 1)
                navigation_ok = false;
            if (n == 40 || n == 43)
                meter_host_click(685, 450, n == 40);
            if (n == 50 && demo_ui_active_page(ui) != 3)
                navigation_ok = false;
            if (n == 55 || n == 58)
                meter_host_click(600, 155, n == 55);
            if (n == 70 || n == 73)
                meter_host_click(490, 450, n == 70);
            if (n == 80 && demo_ui_active_page(ui) != 2)
                navigation_ok = false;
            if (n == 90 || n == 93)
                meter_host_click(100, 450, n == 90);
        }
        if (!meter_host_events())
            break;
        if (fixture) { }
        else if (visual)
        {
            float f = !strcmp(visual, "min") ? 0 : !strcmp(visual, "mid") ? 0.5f : 1;
            meter_synthetic_values(&runtime, now, 50 * f, 10 + 90 * f, -45 + 90 * f);
        }
        else if (n % 6 == 0)
            meter_synthetic_step(&runtime, now, scenario);
        meter_runtime_poll(&runtime, 8);
        meter_core_connection(&core, runtime.connected, runtime.generation);
        meter_core_tick(&core, now, 750);
        if (product->evaluate)
            product->evaluate(&core.snapshot);
        lv_tick_inc(16);
        uint64_t start = meter_host_counter();
        product->ui->present(ui, &core.snapshot, 16);
        lv_timer_handler();
        double us = meter_host_us(start, meter_host_counter());
        sum_us += us;
        if (us > max_us)
            max_us = us;
        lv_mem_monitor_t mem;
        lv_mem_monitor(&mem);
        size_t used = mem.total_size - mem.free_size;
        if (used > heap_peak)
            heap_peak = used;
        ++completed;
        if (!frames)
            meter_host_delay(16);
    }
    bool pass = navigation_ok && meter_ui_object_count(lv_screen_active()) == objects;
    if (smoke && !core.snapshot.imperial)
        pass = false;
    if (capture && !meter_host_capture(capture))
        pass = false;
    /* 故障按身份逐个列出而非压成位图：身份是产品句柄，不是位序号。 */
    char fault_ids[DEMO_FAULT_SLOTS * 7 + 1] = "";
    unsigned active_faults = 0;
    for (size_t i = 0; i < core.snapshot.catalog->fault_count; ++i)
        if (core.snapshot.faults[i].active)
        {
            size_t used = strlen(fault_ids);
            snprintf(fault_ids + used, sizeof(fault_ids) - used, "%s%u", used ? "," : "",
                     (unsigned)core.snapshot.faults[i].id);
            ++active_faults;
        }
    meter_value_t speed = meter_snapshot_read(&core.snapshot, METER_SPEED);
    printf("{\"result\":\"%s\",\"frames\":%u,\"objects\":%u,\"objects_final\":%u,"
           "\"heap_high_water\":%u,\"update_us_avg\":%.2f,\"update_us_max\":%.2f,"
           "\"dispatched\":%u,\"decode_failed\":%u,\"overflow\":%u,\"faults\":%u,"
           "\"fault_ids\":[%s],"
           "\"speed_state\":%u,\"speed\":%.2f,\"imperial\":%s,\"language\":\"%s\",\"page\":%u,\"ge_"
           "hits\":null,\"sw_fallbacks\":null}\n",
           pass ? "PASS" : "FAIL", completed, (unsigned)objects,
           (unsigned)meter_ui_object_count(lv_screen_active()), (unsigned)heap_peak,
           completed ? sum_us / completed : 0, max_us, (unsigned)runtime.diagnostics.dispatched,
           (unsigned)runtime.diagnostics.decode_failed, (unsigned)runtime.diagnostics.overflow,
           active_faults, fault_ids, (unsigned)speed.state, (double)speed.value,
           core.snapshot.imperial ? "true" : "false",
           core.snapshot.language == METER_LANGUAGE_ZH ? "zh-CN" : "en", demo_ui_active_page(ui));
    product->ui->destroy(ui);
    meter_host_close();
    lv_deinit();
    return pass ? 0 : 7;
}
