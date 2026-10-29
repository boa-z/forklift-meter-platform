#include "sim/product_host.h"
#include "sim/domain_fixture.h"
#include "runtime/hmi_periodic.h"
#include "platform/host/host_platform.h"
#include "contracts/hmi_wall_clock.h"
#include "ui/common/i18n/hmi_i18n_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool action(void *ctx, const hmi_action_t *a)
{
    return hmi_product_host_action(ctx, hmi_product_get(), a, lv_tick_get());
}
/* 主机没有硬件 RTC，也不该读到宿主机时间：截图与断言需要逐帧可复现。
   固定源可写，用来覆盖 Product 的对时流程。 */
static hmi_wall_time_t sim_utc = {2026u, 10u, 1u, 0u, 30u, 0u, true};
static bool sim_clock_read(hmi_wall_time_t *out, void *context)
{
    (void)context;
    *out = sim_utc;
    return true;
}
static bool sim_clock_write(const hmi_wall_time_t *utc, void *context)
{
    (void)context;
    /* 与真实端口一致：日历字段越界由公共层拒绝，这里只接受合法读数。 */
    sim_utc = *utc;
    sim_utc.valid = true;
    return true;
}
int hmi_product_host(int argc, char **argv, void (*step)(hmi_runtime_t *, uint32_t))
{
    unsigned frames = 0;
    bool hidden = false;
    const char *capture = NULL, *fixture = NULL;
    hmi_language_t language = HMI_LANGUAGE_EN;
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
            language = !strcmp(name, "chinese") ? HMI_LANGUAGE_ZH : HMI_LANGUAGE_EN;
            language_override = true;
        }
        else return 2;
    }
    const hmi_product_t *p = hmi_product_get();
    if (p->prepare && !p->prepare()) return 3;
    const hmi_catalog_t *c = p->catalog;
    hmi_core_storage_t s = {
        calloc(c->signal_count, sizeof(hmi_value_t)), c->signal_count,
        calloc(c->parameter_count ? c->parameter_count : 1, sizeof(float)), c->parameter_count,
        calloc(c->fault_count ? c->fault_count : 1, sizeof(hmi_fault_state_t)), c->fault_count};
    hmi_core_t core; hmi_runtime_t runtime;
    if (!s.signals || !s.parameters || !s.faults ||
        !hmi_core_init_with_settings(&core,c,&s,p->initial_settings) ||
        !hmi_runtime_init(&runtime,p,hmi_core_apply,&core)) return 3;
    if (language_override) core.snapshot.language = language;
    language = core.snapshot.language;
    hmi_runtime_connection(&runtime,true);
    hmi_core_connection(&core,true,runtime.generation);
    if (p->app_reset) p->app_reset(runtime.generation);
    /* 主机没有发送硬件：周期帧照常编码以覆盖 Product 的编码器，结果丢弃。 */
    hmi_deadline_t periodic[8] = {0};
    if (fixture && !hmi_fixture_load(&core,fixture)) return 4;
    lv_init();
    if (!hmi_i18n_init() || !hmi_host_open(hidden)) return 5;
    if (p->start && !p->start()) return 5;
    (void)hmi_i18n_set_language(language);
    /* 端口层在固件里由 INIT_APP_EXPORT 绑定；主机侧必须显式绑定这个固定源。 */
    hmi_wall_clock_bind(sim_clock_read, NULL);
    hmi_wall_clock_bind_set(sim_clock_write, NULL);
    hmi_ui_actions_t actions = {action,&core};
    void *ui = p->ui->create(lv_screen_active(),&actions);
    if (!ui) return 6;
    /* 目录接受的每个语言字节都要有 Product 注册的语言，否则已存的设置会落到没有译文的编号上。 */
    if (hmi_i18n_language_count() < hmi_catalog_language_count(c)) return 7;
    unsigned n;
    for (n=0; (!frames || n<frames) && hmi_host_events(); ++n)
    {
        uint32_t now=n*16;
        if (!fixture && step) step(&runtime,now);
        hmi_runtime_step(&runtime,&core,now,true);
        for (size_t i = 0; i < p->periodic_count && i < 8u; ++i)
        {
            hmi_can_frame_t frame;
            (void)hmi_periodic_due(&periodic[i], &p->periodic[i], &core.snapshot, now, true, false, &frame);
        }
        (void)hmi_i18n_set_language(core.snapshot.language);
        p->ui->present(ui,&core.snapshot,now);
        lv_tick_inc(16); lv_timer_handler();
        if(!frames) hmi_host_delay(16);
    }
    bool pass = !runtime.diagnostics.decode_failed && (!capture || hmi_host_capture(capture));
    printf("{\"result\":\"%s\",\"frames\":%u,\"revision\":%u,\"dispatched\":%u,\"language\":\"%s\"}\n",
           pass ? "PASS" : "FAIL",n,(unsigned)core.snapshot.revision,(unsigned)runtime.diagnostics.dispatched,
           hmi_i18n_language_code(core.snapshot.language));
    p->ui->destroy(ui); hmi_host_close(); lv_deinit();
    free(s.signals);free(s.parameters);free(s.faults);
    return pass ? 0 : 8;
}
