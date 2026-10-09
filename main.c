/* SPDX-License-Identifier: Apache-2.0 */
/* UI 独占 LVGL；App 独占 Domain，协议/发送/NVM/升级由各自 owner 驱动。 */
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
#include <stdio.h>
#include <string.h>
#include <ulog.h>
#include "meter_display_version.h"
#ifdef METER_ENABLE_CAN_UPDATE
#include "meter_update_build.h"
#include "platform/rtthread/meter_update_port.h"
#endif
#ifndef LPKG_LVGL_THREAD_STACK_SIZE
#define LPKG_LVGL_THREAD_STACK_SIZE 32768
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
    { LOG_E("Product composition failed"); return; }
    meter_core_storage_t bound = composition.domain;
    meter_core_storage_t ui_bound = composition.ui;
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
    if (METER_FIRMWARE_DISPLAY_VERSION[0]) identity.display_version = METER_FIRMWARE_DISPLAY_VERSION;
    if (!meter_debug_init(&diagnostics, &identity)) return;
    lv_init();
    meter_debug_lvgl_log_init();
    meter_rtthread_board_port_t board = meter_board_port(&ui_diagnostics);
    if (!meter_i18n_init() || !composition.locale_init() ||
        !meter_core_init_with_settings(&core, product->catalog, &bound, product->initial_settings) ||
        !board.display_init(board.context) || !board.touch_init(board.context))
    { LOG_E("UI/core startup failed"); return; }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    static const meter_ui_actions_t actions = {.send = meter_execution_action};
    void *ui = product->ui->create(screen, &actions);
    if (!ui) { LOG_E("UI allocation failed"); meter_board_close(&ui_diagnostics); lv_deinit(); return; }
    meter_execution_config_t config = {.product = product, .core = &core,
        .published_storage = composition.presentation,
        .diagnostic_storage = composition.diagnostic, .public_diagnostics = &diagnostics};
    if (!meter_execution_start(&config)) { LOG_E("runtime startup failed"); product->ui->destroy(ui); meter_board_close(&ui_diagnostics); lv_deinit(); return; }
    LOG_I("product=%s; runtime=production; UI/Protocol/App owners separated", product->id);
    /* present() 要的是累计运行毫秒（开机停留、闪烁相位都在 UI 里按它计时），
       不是帧间隔：传 16/50 这类增量会让开机页永远停在原地。 */
    const uint32_t ui_start = meter_board_now_ms();
    ui_diagnostics.data.ui.available = true;
    while (!meter_execution_ui_shutdown_requested())
    {
        uint32_t now = meter_board_now_ms();
        bool normal = false;
        meter_update_view_t update;
        if (meter_execution_present(&snapshot, &ui_bound, &update, &normal))
        {
#ifndef METER_ENABLE_CAN_UPDATE
            /* 没有 CAN 升级端点时没人填更新视图，但软件版本页仍要显示本次构建的版本。 */
            memset(&update, 0, sizeof(update));
            snprintf(update.current_version, sizeof(update.current_version), "%s", METER_FIRMWARE_DISPLAY_VERSION);
#endif
            if (product->ui->present_update) product->ui->present_update(ui, &update, snapshot.language);
            if (normal)
            {
                product->ui->present(ui, &snapshot, now - ui_start);
                meter_diag_increment(&ui_diagnostics.data.ui.present_count);
            }
        }
        meter_request_result_t result;
        if (meter_execution_action_result(&result) && result.code != METER_RESULT_APPLIED)
            LOG_W("UI intention rejected by App mode or validation");
        (void)lv_timer_handler();
        meter_board_diagnostics(&ui_diagnostics);
        meter_execution_ui_diagnostics(&ui_diagnostics);
        meter_debug_log_drain();
        rt_thread_mdelay(normal ? 16 : 50);
    }
    product->ui->destroy(ui);
    meter_board_close(&ui_diagnostics);
    lv_deinit();
    meter_execution_ui_diagnostics(&ui_diagnostics);
    meter_execution_ui_stopped();
    LOG_I("UI teardown acknowledged; App completes runtime stop");
}
int main(void)
{
    static struct rt_thread ui_thread;
    static rt_ubase_t stack[LPKG_LVGL_THREAD_STACK_SIZE / sizeof(rt_ubase_t)];
    if (rt_thread_init(&ui_thread, "meter_ui", meter_thread, RT_NULL, stack, sizeof(stack),
                       LPKG_LVGL_THREAD_PRIO + 2, 10) != RT_EOK) return -1;
    return rt_thread_startup(&ui_thread) == RT_EOK ? 0 : -1;
}
