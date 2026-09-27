#include "platform/host/host_platform.h"
#include "products/demo/product.h"
#include "runtime/meter_runtime.h"
#include "sim/synthetic.h"
#include "ui/common/widgets/meter_widgets.h"
#include "ui/products/demo/demo_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct
{
    meter_core_t *core;
    const char *path;
    const meter_product_t *product;
} action_context_t;
static bool action(void *context, const meter_action_t *a)
{
    action_context_t *c = context;
    if (!c->product->auth->local_settings ||
        (a->kind == METER_ACTION_PARAMETER && !c->product->capabilities->parameter_write))
        return false;
    meter_core_t candidate = *c->core;
    if (!meter_core_action(&candidate, a) || !meter_host_save(&candidate, c->path))
        return false;
    *c->core = candidate;
    return true;
}
int main(int argc, char **argv)
{
    unsigned frames = 0, page = 0;
    bool smoke = false, hidden = false;
    const char *capture = NULL, *settings = NULL, *visual = NULL, *set_units = NULL;
    demo_scenario_t scenario = DEMO_NORMAL;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--smoke"))
        {
            smoke = true;
            hidden = true;
            frames = 240;
        }
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
                   "file] [--set-units metric|imperial]\n");
            return !strcmp(argv[i], "--help") ? 0 : 2;
        }
    }
    if (page > 3 || (visual && strcmp(visual, "min") && strcmp(visual, "mid") && strcmp(visual, "max")) ||
        (set_units && strcmp(set_units, "metric") && strcmp(set_units, "imperial")))
        return 2;
    const meter_product_t *product = meter_product_get();
    meter_core_t core;
    meter_runtime_t runtime;
    if (!meter_core_init(&core, product->catalog) ||
        !meter_runtime_init(&runtime, product, meter_core_apply, &core))
        return 3;
    meter_host_load(&core, settings);
    action_context_t act = {&core, settings, product};
    meter_ui_actions_t actions = {action, &act};
    if (set_units)
    {
        meter_action_t a = {METER_ACTION_UNITS, 0, !strcmp(set_units, "imperial")};
        if (!action(&act, &a))
            return 4;
    }
    lv_init();
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
        if (visual)
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
    printf("{\"result\":\"%s\",\"frames\":%u,\"objects\":%u,\"objects_final\":%u,"
           "\"heap_high_water\":%u,\"update_us_avg\":%.2f,\"update_us_max\":%.2f,"
           "\"dispatched\":%u,\"decode_failed\":%u,\"overflow\":%u,\"faults\":%u,"
           "\"speed_state\":%u,\"speed\":%.2f,\"imperial\":%s,\"page\":%u,\"ge_"
           "hits\":null,\"sw_fallbacks\":null}\n",
           pass ? "PASS" : "FAIL", completed, (unsigned)objects,
           (unsigned)meter_ui_object_count(lv_screen_active()), (unsigned)heap_peak,
           completed ? sum_us / completed : 0, max_us, (unsigned)runtime.diagnostics.dispatched,
           (unsigned)runtime.diagnostics.decode_failed, (unsigned)runtime.diagnostics.overflow,
           (unsigned)core.snapshot.active_faults, (unsigned)core.snapshot.signals[METER_SPEED].state,
           (double)core.snapshot.signals[METER_SPEED].value, core.snapshot.imperial ? "true" : "false",
           demo_ui_active_page(ui));
    product->ui->destroy(ui);
    meter_host_close();
    lv_deinit();
    return pass ? 0 : 7;
}
