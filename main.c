/* SPDX-License-Identifier: Apache-2.0 */
/* SDK 的 LVGL 线程独占 LVGL 与 UI；运行时线程独占 Domain 与协议，设置介质与升级 Flash 各有独立 worker。 */
#define LOG_TAG "meter.boot"
#define LOG_LVL LOG_LVL_INFO
#include "meter_build_identity.h"
#include "platform/rtthread/debug/meter_debug_console.h"
#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_execution_port.h"
#include "contracts/meter_firmware.h"
#include "contracts/meter_product.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <lvgl.h>
#include <rtthread.h>
#include <ulog.h>
#include "meter_display_version.h"
#ifdef METER_ENABLE_CAN_UPDATE
#include "meter_update_build.h"
#endif
#define METER_STRING_IMPL(x) #x
#define METER_STRING(x) METER_STRING_IMPL(x)

static meter_core_t core;
static meter_diagnostics_t diagnostics, ui_diagnostics;
static meter_build_info_t identity;
/* UI 周期任务的上下文：只在 LVGL 线程里读写。 */
static struct
{
    const meter_product_t *product;
    meter_firmware_composition_t composition;
    meter_snapshot_t snapshot;
    void *ui;
    uint32_t ui_start;
} app;

static void present_tick(lv_timer_t *timer)
{
    const uint32_t now = meter_board_now_ms();
    bool normal = false;
    meter_update_view_t update;
    if (meter_execution_present(&app.snapshot, &app.composition.ui, &update, &normal))
    {
        /* 语言只有一个来源：持久化的快照。UI 与升级页都在这之后才取词，已绑定的标签随之刷新。 */
        (void)meter_i18n_set_language(app.snapshot.language);
        if (app.product->ui->present_update)
            app.product->ui->present_update(app.ui, &update, app.snapshot.language);
        if (normal)
        {
            /* present() 要的是累计运行毫秒（开机停留、闪烁相位都在 UI 里按它计时），
               不是帧间隔：传 16/50 这类增量会让开机页永远停在原地。 */
            app.product->ui->present(app.ui, &app.snapshot, now - app.ui_start);
            meter_diag_increment(&ui_diagnostics.data.ui.present_count);
        }
    }
    meter_board_diagnostics(&ui_diagnostics);
    meter_execution_ui_diagnostics(&ui_diagnostics);
    meter_debug_log_drain();
    lv_timer_set_period(timer, normal ? 16 : 50);
}

/* 整个 UI 启动与周期呈现都留在 SDK 的 LVGL 线程里，由它的主循环运行 lv_timer_handler()。 */
static void meter_gui_start(void)
{
    app.product = meter_product_get();
    const meter_product_t *product = app.product;
    if (!meter_firmware_compose(&app.composition) || !app.composition.locale_init ||
        (product->prepare && !product->prepare()))
    {
        LOG_E("Product composition failed");
        return;
    }
    meter_diagnostics_init(&diagnostics);
    meter_diagnostics_init(&ui_diagnostics);
    identity = (meter_build_info_t){.product = product->id, .platform_revision = METER_BUILD_PLATFORM,
        .sdk_revision = METER_BUILD_SDK,
        .lvgl_version = METER_STRING(LVGL_VERSION_MAJOR) "." METER_STRING(LVGL_VERSION_MINOR) "." METER_STRING(LVGL_VERSION_PATCH),
        .board = METER_BUILD_BOARD, .build_date = __DATE__, .build_time = __TIME__};
#ifdef METER_ENABLE_CAN_UPDATE
    identity.firmware_version = METER_UPDATE_FIRMWARE_VERSION;
#endif
    if (METER_FIRMWARE_DISPLAY_VERSION[0])
        identity.display_version = METER_FIRMWARE_DISPLAY_VERSION;
    if (!meter_debug_init(&diagnostics, &identity))
        return;
    meter_debug_lvgl_log_init();
    if (!meter_i18n_init() || !app.composition.locale_init() ||
        !meter_core_init_with_settings(&core, product->catalog, &app.composition.domain, product->initial_settings) ||
        !meter_board_display_init() || (product->start && !product->start()))
    {
        LOG_E("UI/core startup failed");
        return;
    }
    static const meter_ui_actions_t actions = {.send = meter_execution_action};
    app.ui = product->ui->create(lv_screen_active(), &actions);
    const meter_execution_config_t config = {.product = product, .core = &core,
        .published_storage = app.composition.presentation, .public_diagnostics = &diagnostics};
    if (!app.ui || !meter_execution_start(&config))
    {
        LOG_E("runtime startup failed");
        if (app.ui)
            product->ui->destroy(app.ui);
        app.ui = NULL;
        meter_board_display_close(&ui_diagnostics);
        return;
    }
    if (meter_i18n_language_count() < meter_catalog_language_count(product->catalog))
        LOG_E("catalog accepts %u languages, Product registered %u", meter_catalog_language_count(product->catalog),
              (unsigned)meter_i18n_language_count());
    LOG_I("product=%s; UI running in the LVGL thread", product->id);
    app.ui_start = meter_board_now_ms();
    ui_diagnostics.data.ui.available = true;
    if (!lv_timer_create(present_tick, 16, NULL))
        LOG_E("UI present timer unavailable");
}

/* SDK 的 LVGL 线程在显示与输入设备初始化之后调用 lv_user_gui_init()。SConscript 用链接器的
   -Wl,--wrap=lv_user_gui_init 把这个调用接到下面的函数上：先跑 SDK 原来的实现，再启动本应用的 UI，
   不需要改动 SDK 源码。 */
extern void __real_lv_user_gui_init(void);
void __wrap_lv_user_gui_init(void)
{
    __real_lv_user_gui_init();
    meter_gui_start();
}

/* 启动全部在 LVGL 线程里完成；RT-Thread 的主线程不需要做事。 */
int main(void)
{
    return 0;
}
