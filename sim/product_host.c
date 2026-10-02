#include "sim/product_host.h"
#include "sim/domain_fixture.h"
#include "platform/host/host_platform.h"
#include "contracts/meter_wall_clock.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool action(void *ctx, const meter_action_t *a)
{
    return meter_product_host_action(ctx, meter_product_get(), a, lv_tick_get());
}
/* 主机没有硬件 RTC，也不该读到宿主机时间：截图与断言需要逐帧可复现。
   固定源可写，用来覆盖 Product 的对时流程。 */
static meter_wall_time_t sim_utc = {2026u, 10u, 1u, 0u, 30u, 0u, true};
static bool sim_clock_read(meter_wall_time_t *out, void *context)
{
    (void)context;
    *out = sim_utc;
    return true;
}
static bool sim_clock_write(const meter_wall_time_t *utc, void *context)
{
    (void)context;
    /* 与真实端口一致：日历字段越界由公共层拒绝，这里只接受合法读数。 */
    sim_utc = *utc;
    sim_utc.valid = true;
    return true;
}
int meter_product_host(int argc, char **argv, void (*step)(meter_runtime_t *, uint32_t))
{
    unsigned frames = 0;
    bool hidden = false;
    const char *capture = NULL, *fixture = NULL;
    meter_language_t language = METER_LANGUAGE_EN;
    bool language_override = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--smoke")) { hidden = true; frames = 120; }
        else if (!strcmp(argv[i], "--hidden")) hidden = true;
        else if (!strcmp(argv[i], "--frames") && i+1 < argc) frames = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--capture") && i+1 < argc) capture = argv[++i];
        else if (!strcmp(argv[i], "--fixture") && i+1 < argc) fixture = argv[++i];
        else if (!strcmp(argv[i], "--set-language") && i+1 < argc)
        {
            const char *name = argv[++i];
            if (strcmp(name, "english") && strcmp(name, "chinese")) return 2;
            language = !strcmp(name, "chinese") ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN;
            language_override = true;
        }
        else return 2;
    }
    const meter_product_t *p = meter_product_get();
    const meter_catalog_t *c = p->catalog;
    meter_core_storage_t s = {
        calloc(c->signal_count, sizeof(meter_value_t)), c->signal_count,
        calloc(c->parameter_count ? c->parameter_count : 1, sizeof(float)), c->parameter_count,
        calloc(c->fault_count ? c->fault_count : 1, sizeof(meter_fault_state_t)), c->fault_count};
    meter_core_t core; meter_runtime_t runtime;
    if (!s.signals || !s.parameters || !s.faults ||
        !meter_core_init_with_settings(&core,c,&s,p->initial_settings) ||
        !meter_runtime_init(&runtime,p,meter_core_apply,&core)) return 3;
    if (language_override) core.snapshot.language = language;
    language = core.snapshot.language;
    meter_runtime_connection(&runtime,true);
    meter_core_connection(&core,true,runtime.generation);
    if (p->app_reset) p->app_reset(runtime.generation);
    if (fixture && !meter_fixture_load(&core,fixture)) return 4;
    lv_init();
    if (!meter_i18n_init() || !meter_host_open(hidden)) return 5;
    /* 端口层在固件里由 INIT_APP_EXPORT 绑定；主机侧必须显式绑定这个固定源。 */
    meter_wall_clock_bind(sim_clock_read, NULL);
    meter_wall_clock_bind_set(sim_clock_write, NULL);
    meter_ui_actions_t actions = {action,&core};
    void *ui = p->ui->create(lv_screen_active(),&actions);
    if (!ui) return 6;
    unsigned n;
    for (n=0; (!frames || n<frames) && meter_host_events(); ++n)
    {
        uint32_t now=n*16;
        if (!fixture && step) step(&runtime,now);
        meter_runtime_poll(&runtime,32);
        if (!meter_runtime_process(&runtime,now)) return 7;
        meter_core_tick(&core, now);
        if(p->app_tick) p->app_tick(&core.snapshot,now);
        if(p->evaluate) p->evaluate(&core.snapshot);
        p->ui->present(ui,&core.snapshot,now);
        lv_tick_inc(16); lv_timer_handler();
        if(!frames) meter_host_delay(16);
    }
    bool pass = !runtime.diagnostics.decode_failed && (!capture || meter_host_capture(capture));
    printf("{\"result\":\"%s\",\"frames\":%u,\"revision\":%u,\"dispatched\":%u,\"language\":\"%s\"}\n",
           pass ? "PASS" : "FAIL",n,(unsigned)core.snapshot.revision,(unsigned)runtime.diagnostics.dispatched,
           language==METER_LANGUAGE_ZH ? "zh-CN":"en");
    p->ui->destroy(ui); meter_host_close(); lv_deinit();
    free(s.signals);free(s.parameters);free(s.faults);
    return pass ? 0 : 8;
}
