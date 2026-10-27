/* SPDX-License-Identifier: Apache-2.0 */
/* UI 线程独占 LVGL；运行时线程独占 Domain 与协议，设置介质与升级 Flash 各有独立 worker。 */
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
#ifndef AIC_FORKLIFT_UI_THREAD_STACK_SIZE
#define AIC_FORKLIFT_UI_THREAD_STACK_SIZE 32768
#endif
#ifndef LPKG_LVGL_THREAD_PRIO
#define LPKG_LVGL_THREAD_PRIO 20
#endif
#define METER_STRING_IMPL(x) #x
#define METER_STRING(x) METER_STRING_IMPL(x)

static meter_core_t core;
static meter_diagnostics_t diagnostics, ui_diagnostics;
static meter_build_info_t identity;
static void meter_thread(void *parameter)
{
    (void)parameter;
    const meter_product_t *product = meter_product_get();
    meter_firmware_composition_t composition;
    if (!meter_firmware_compose(&composition) || !composition.locale_init)
    {
        LOG_E("Product composition failed");
        return;
    }
    meter_snapshot_t snapshot = {0};
    meter_diagnostics_init(&diagnostics);
    meter_diagnostics_init(&ui_diagnostics);
    identity = (meter_build_info_t){.product = product->id, .platform_revision = METER_BUILD_PLATFORM,
        .sdk_revision = METER_BUILD_SDK,
        .lvgl_version = METER_STRING(LVGL_VERSION_MAJOR) "." METER_STRING(LVGL_VERSION_MINOR) "." METER_STRING(LVGL_VERSION_PATCH),
        .lvgl_aic_revision = METER_BUILD_LVGL_AIC, .board = METER_BUILD_BOARD,
        .build_date = __DATE__, .build_time = __TIME__};
#ifdef METER_ENABLE_CAN_UPDATE
    identity.firmware_version = METER_UPDATE_FIRMWARE_VERSION;
#endif
    if (METER_FIRMWARE_DISPLAY_VERSION[0])
        identity.display_version = METER_FIRMWARE_DISPLAY_VERSION;
    if (!meter_debug_init(&diagnostics, &identity))
        return;
    lv_init();
    meter_debug_lvgl_log_init();
    if (!meter_i18n_init() || !composition.locale_init() ||
        !meter_core_init_with_settings(&core, product->catalog, &composition.domain, product->initial_settings) ||
        !meter_board_display_init())
    {
        LOG_E("UI/core startup failed");
        return;
    }
    static const meter_ui_actions_t actions = {.send = meter_execution_action};
    void *ui = product->ui->create(lv_screen_active(), &actions);
    const meter_execution_config_t config = {.product = product, .core = &core,
        .published_storage = composition.presentation, .public_diagnostics = &diagnostics};
    if (!ui || !meter_execution_start(&config))
    {
        LOG_E("runtime startup failed");
        if (ui)
            product->ui->destroy(ui);
        meter_board_display_close(&ui_diagnostics);
        lv_deinit();
        return;
    }
    LOG_I("product=%s; UI thread running", product->id);
    /* present() 要的是累计运行毫秒（开机停留、闪烁相位都在 UI 里按它计时），
       不是帧间隔：传 16/50 这类增量会让开机页永远停在原地。 */
    const uint32_t ui_start = meter_board_now_ms();
    ui_diagnostics.data.ui.available = true;
    for (;;)
    {
        uint32_t now = meter_board_now_ms();
        bool normal = false;
        meter_update_view_t update;
        if (meter_execution_present(&snapshot, &composition.ui, &update, &normal))
        {
            /* 语言只有一个来源：持久化的快照。UI 与升级页都在这之后才取词，LVGL 随之刷新已绑定的标签。 */
            (void)meter_i18n_set_language(snapshot.language);
            if (product->ui->present_update)
                product->ui->present_update(ui, &update, snapshot.language);
            if (normal)
            {
                product->ui->present(ui, &snapshot, now - ui_start);
                meter_diag_increment(&ui_diagnostics.data.ui.present_count);
            }
        }
        (void)lv_timer_handler();
        meter_board_diagnostics(&ui_diagnostics);
        meter_execution_ui_diagnostics(&ui_diagnostics);
        meter_debug_log_drain();
        rt_thread_mdelay(normal ? 16 : 50);
    }
}
int main(void)
{
    static struct rt_thread ui_thread;
    static rt_ubase_t stack[AIC_FORKLIFT_UI_THREAD_STACK_SIZE / sizeof(rt_ubase_t)];
    if (rt_thread_init(&ui_thread, "meter_ui", meter_thread, RT_NULL, stack, sizeof(stack),
                       LPKG_LVGL_THREAD_PRIO + 2, 10) != RT_EOK)
        return -1;
    return rt_thread_startup(&ui_thread) == RT_EOK ? 0 : -1;
}
