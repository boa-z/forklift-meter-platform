/* SPDX-License-Identifier: Apache-2.0 */
/* Demo smoke 由一个 owner 线程驱动 LVGL、协议和 Domain；生产线程分工见架构文档。 */
#define LOG_TAG "meter.boot"
#define LOG_LVL LOG_LVL_INFO
#include "meter_build_identity.h"
#include "platform/rtthread/debug/meter_debug_console.h"
#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_nvm_port.h"
#include "product/demo_storage.h"
#include "product/product.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/demo_i18n.h"
#include <lvgl.h>
#include <rtthread.h>
#include <ulog.h>
#ifndef LPKG_LVGL_THREAD_STACK_SIZE
#define LPKG_LVGL_THREAD_STACK_SIZE 32768
#endif
#ifndef LPKG_LVGL_THREAD_PRIO
#define LPKG_LVGL_THREAD_PRIO 20
#endif
#define METER_STRING_IMPL(x) #x
#define METER_STRING(x) METER_STRING_IMPL(x)
static meter_rtthread_adapter_t adapter;
static meter_core_t core;
static demo_domain_store_t storage;
static meter_diagnostics_t diagnostics;
static meter_build_info_t identity;
static bool board_action(void *ctx, const meter_action_t *action)
{
    const meter_product_t *product = meter_product_get();
    if (!product->auth->local_settings ||
        (action->kind == METER_ACTION_PARAMETER && !product->capabilities->parameter_write))
        return false;
    if (!meter_board_nvm_ready() || !meter_core_action(ctx, action))
        return false;
    /* RAM 应用和 durable 分开发布，保存失败不伪造回滚。 */
    (void)meter_board_nvm_changed(rt_tick_get_millisecond());
    return true;
}
static void meter_thread(void *parameter)
{
    (void)parameter;
    const meter_product_t *product = meter_product_get();
    meter_core_storage_t bound = demo_domain_bind(&storage);
    meter_diagnostics_init(&diagnostics);
    identity = (meter_build_info_t){product->id,
                                    METER_BUILD_PLATFORM,
                                    METER_BUILD_SDK,
                                    METER_STRING(LVGL_VERSION_MAJOR) "." METER_STRING(
                                        LVGL_VERSION_MINOR) "." METER_STRING(LVGL_VERSION_PATCH),
                                    METER_BUILD_LVGL_AIC,
                                    METER_BUILD_BOARD,
                                    __DATE__,
                                    __TIME__};
    if (!meter_debug_init(&diagnostics, &identity))
    {
        LOG_E("diagnostics mutex init failed");
        return;
    }
    meter_debug_lock();
    meter_rtthread_board_port_t board = meter_board_port(&diagnostics);
    meter_ui_actions_t actions = {board_action, &core};
    lv_init();
    meter_debug_lvgl_log_init();
    LOG_I("boot platform=%s", METER_BUILD_PLATFORM);
    if (!meter_i18n_init() || !demo_i18n_init() || !meter_core_init(&core, product->catalog, &bound))
    {
        LOG_E("core/i18n initialization failed");
        goto failed;
    }
    meter_core_bind_diagnostics(&core, &diagnostics);
    if (!meter_rtthread_adapter_init(&adapter, product, &core, &board))
    {
        LOG_E("board/runtime initialization failed");
        goto failed;
    }
    bool nvm_started = product->storage && product->storage->enabled && meter_board_nvm_start(&core);
    if (product->storage && product->storage->enabled && !nvm_started)
        LOG_E("NVM worker initialization failed; settings remain RAM-only");
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    void *ui = product->ui->create(screen, &actions);
    if (!ui)
    {
        LOG_E("UI allocation failed");
        goto failed;
    }
    LOG_I("product=%s; English/Chinese enabled; settings=%s", product->id,
          nvm_started ? "async-nvm" : "RAM-only");
    uint32_t previous = board.now_ms(board.context);
    diagnostics.data.ui.available = true;
    meter_debug_unlock();
    bool first_frame = false;
    for (;;)
    {
        meter_debug_lock();
        uint32_t now = board.now_ms(board.context);
        meter_diagnostics_time(&diagnostics, now);
        meter_rtthread_adapter_poll(&adapter, 8);
        meter_board_nvm_poll(now, &diagnostics.data.storage);
        product->ui->present(ui, &core.snapshot, (uint32_t)(now - previous));
        meter_diag_increment(&diagnostics.data.ui.present_count);
        previous = now;
        lv_timer_handler();
        meter_board_diagnostics(&diagnostics);
        if (!first_frame && diagnostics.data.ui.flush_count > 0U)
        {
            LOG_I("first frame flushed; board visual/touch verification still required");
            first_frame = true;
        }
        meter_debug_unlock();
        meter_debug_log_drain();
        rt_thread_mdelay(16);
    }
failed:
    meter_board_close(&diagnostics);
    lv_deinit();
    meter_debug_unlock();
}
int main(void)
{
    rt_thread_t thread = rt_thread_create("meter_demo", meter_thread, RT_NULL, LPKG_LVGL_THREAD_STACK_SIZE,
                                          LPKG_LVGL_THREAD_PRIO, 10);
    if (!thread)
        return -1;
    if (rt_thread_startup(thread) != RT_EOK)
    {
        rt_thread_delete(thread);
        return -1;
    }
    return 0;
}
