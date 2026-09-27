/* SPDX-License-Identifier: Apache-2.0 */
/* Public Demo board test: one UI thread owns LVGL, protocol and domain state. */
#define LOG_TAG "meter.demo"
#define LOG_LVL LOG_LVL_INFO
#include "lv_aic_display.h"
#include "lv_aic_indev.h"
#include "lvgl_aic.h"
#include "platform/rtthread/meter_rtthread_adapter.h"
#include "product/demo_storage.h"
#include "product/product.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/demo_i18n.h"
#include <lvgl.h>
#include <mpp_fb.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>
#include <ulog.h>
#ifndef LPKG_LVGL_THREAD_STACK_SIZE
#define LPKG_LVGL_THREAD_STACK_SIZE 32768
#endif
#ifndef LPKG_LVGL_THREAD_PRIO
#define LPKG_LVGL_THREAD_PRIO 20
#endif
static meter_rtthread_adapter_t adapter;
static meter_core_t core;
static demo_domain_store_t storage;
static rt_device_t can_devices[METER_BUS_COUNT];
static unsigned next_bus;
static uint32_t board_now(void *ctx)
{
    (void)ctx;
    return (uint32_t)rt_tick_get_millisecond();
}
/* This standalone Demo owns the entire display. Video applications must use
 * their own layer policy instead of inheriting this exclusive takeover. */
static bool board_display_takeover(void)
{
    struct mpp_fb *fb = mpp_fb_open();
    if (!fb)
        return false;
    struct aicfb_layer_data layer = {.layer_id = AICFB_LAYER_TYPE_VIDEO};
    bool ok = mpp_fb_ioctl(fb, AICFB_UPDATE_LAYER_CONFIG, &layer) == 0;
    layer.layer_id = AICFB_LAYER_TYPE_UI;
    for (unsigned rect = 1; rect < 4; ++rect)
    {
        layer.rect_id = rect;
        if (mpp_fb_ioctl(fb, AICFB_UPDATE_LAYER_CONFIG, &layer) != 0)
            ok = false;
    }
    struct aicfb_alpha_config alpha = {
        .layer_id = AICFB_LAYER_TYPE_UI, .enable = 1, .mode = AICFB_GLOBAL_ALPHA_MODE, .value = 255};
    struct aicfb_ck_config ck = {.layer_id = AICFB_LAYER_TYPE_UI};
    if (mpp_fb_ioctl(fb, AICFB_UPDATE_ALPHA_CONFIG, &alpha) != 0)
        ok = false;
    if (mpp_fb_ioctl(fb, AICFB_UPDATE_CK_CONFIG, &ck) != 0)
        ok = false;
    mpp_fb_close(fb);
    if (ok)
        LOG_I("display exclusive: video/extra UI rects off; alpha=255; color key off");
    else
        LOG_E("display exclusive takeover failed");
    return ok;
}
static bool board_display(void *ctx)
{
    (void)ctx;
    if (!board_display_takeover())
        return false;
    int result = lv_aic_init();
    if (result != LV_AIC_OK)
        LOG_E("display/touch initialization failed: %d", result);
    return result == LV_AIC_OK;
}
static bool board_touch(void *ctx)
{
    (void)ctx;
    return lv_aic_get_pointer_indev() != NULL;
}
static void board_touch_report(void)
{
    lv_aic_touch_diagnostics_t d;
    if (lv_aic_indev_get_diagnostics(lv_aic_get_pointer_indev(), &d) != LV_AIC_OK)
        return;
    /* The board ulog buffer is 128 bytes, including its prefix. */
    LOG_I("TOUCH range=%ldx%ld xy=%d,%d state=%d", (long)d.range_x, (long)d.range_y, (int)d.x, (int)d.y,
          (int)d.state);
    LOG_I("TOUCH irq=%lu reads=%lu events=%lu delivered=%lu", (unsigned long)d.irqs, (unsigned long)d.reads,
          (unsigned long)d.events, (unsigned long)d.deliveries);
    LOG_I("TOUCH recovered=%lu empty=%lu invalid=%lu", (unsigned long)d.recovered,
          (unsigned long)d.empty_reads, (unsigned long)d.invalid_reads);
}
#if LV_USE_LOG
static void board_lvgl_log(lv_log_level_t level, const char *message)
{
    (void)level;
    rt_kprintf("[lvgl] %s", message);
}
#endif
static void board_touch_event(lv_event_t *event)
{
    lv_point_t point;
    lv_indev_t *indev = lv_aic_get_pointer_indev();
    lv_indev_get_point(indev, &point);
    LOG_I("TOUCH %s xy=%ld,%ld", lv_event_get_code(event) == LV_EVENT_PRESSED ? "pressed" : "released",
          (long)point.x, (long)point.y);
}
static bool board_can_open(void *ctx, meter_bus_role_t bus)
{
    (void)ctx;
    static const char *const names[] = {"can0", "can1"};
    if ((unsigned)bus >= METER_BUS_COUNT)
        return false;
    rt_device_t dev = rt_device_find(names[bus]);
    if (!dev || rt_device_open(dev, RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        LOG_E("cannot open %s", names[bus]);
        return false;
    }
    /* Public test bus uses 500 kbit/s, receive only; no vehicle commands. */
    if (rt_device_control(dev, RT_CAN_CMD_SET_BAUD, (void *)CAN500kBaud) != RT_EOK ||
        rt_device_control(dev, RT_DEVICE_CTRL_SET_INT, NULL) != RT_EOK)
    {
        LOG_E("cannot configure %s", names[bus]);
        rt_device_close(dev);
        return false;
    }
    can_devices[bus] = dev;
    LOG_I("%s opened at 500000 bit/s (public Demo RX)", names[bus]);
    return true;
}
static bool board_can_read(void *ctx, meter_can_frame_t *frame)
{
    for (unsigned i = 0; i < METER_BUS_COUNT; ++i)
    {
        unsigned bus = next_bus;
        next_bus = (next_bus + 1U) % METER_BUS_COUNT;
        if (!can_devices[bus])
            continue;
        struct rt_can_msg msg = {0};
        msg.hdr = -1;
        if (rt_device_read(can_devices[bus], 0, &msg, sizeof(msg)) != sizeof(msg))
            continue;
        memset(frame, 0, sizeof(*frame));
        frame->bus = (meter_bus_role_t)bus;
        frame->id = msg.id;
        frame->extended = msg.ide != 0;
        frame->remote = msg.rtr != 0;
        frame->size = msg.len;
        frame->timestamp_ms = board_now(ctx);
        memcpy(frame->data, msg.data, msg.len <= sizeof(frame->data) ? msg.len : sizeof(frame->data));
        return true;
    }
    return false;
}
static bool board_action(void *ctx, const meter_action_t *action)
{
    const meter_product_t *product = meter_product_get();
    if (!product->auth->local_settings ||
        (action->kind == METER_ACTION_PARAMETER && !product->capabilities->parameter_write))
        return false;
    /* This test image keeps preferences in RAM; reboot restores defaults. */
    return meter_core_action(ctx, action);
}
static void meter_thread(void *parameter)
{
    (void)parameter;
    const meter_product_t *product = meter_product_get();
    meter_core_storage_t bound = demo_domain_bind(&storage);
    meter_rtthread_board_port_t board = {.display_init = board_display,
                                         .touch_init = board_touch,
                                         .can_open = board_can_open,
                                         .can_read = board_can_read,
                                         .now_ms = board_now};
    meter_ui_actions_t actions = {board_action, &core};
    lv_init();
#if LV_USE_LOG
    lv_log_register_print_cb(board_lvgl_log);
#endif
    LOG_I("boardfix2 built %s %s", __DATE__, __TIME__);
    if (!meter_i18n_init() || !demo_i18n_init() || !meter_core_init(&core, product->catalog, &bound) ||
        !meter_rtthread_adapter_init(&adapter, product, &core, &board))
    {
        LOG_E("initialization failed");
        goto failed;
    }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    lv_indev_add_event_cb(lv_aic_get_pointer_indev(), board_touch_event, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(lv_aic_get_pointer_indev(), board_touch_event, LV_EVENT_RELEASED, NULL);
    board_touch_report();
    void *ui = product->ui->create(screen, &actions);
    if (!ui)
    {
        LOG_E("UI allocation failed");
        goto failed;
    }
    LOG_I("product=%s; English/Chinese enabled; settings=RAM", product->id);
    uint32_t previous = board_now(NULL), report = previous;
    bool first_frame = false;
    for (;;)
    {
        uint32_t now = board_now(NULL);
        meter_rtthread_adapter_poll(&adapter, 8);
        product->ui->present(ui, &core.snapshot, (uint32_t)(now - previous));
        previous = now;
        lv_timer_handler();
        if (!first_frame && lv_aic_display_flush_count_get() > 0U)
        {
            LOG_I("first frame flushed; board visual/touch verification still required");
            first_frame = true;
        }
        if ((uint32_t)(now - report) >= 5000U)
        {
            const meter_runtime_diagnostics_t *d = meter_rtthread_adapter_diagnostics(&adapter);
            LOG_I("CAN accepted=%lu dispatched=%lu malformed=%lu unrouted=%lu decode_failed=%lu overflow=%lu",
                  (unsigned long)d->accepted, (unsigned long)d->dispatched, (unsigned long)d->malformed,
                  (unsigned long)d->unrouted, (unsigned long)d->decode_failed, (unsigned long)d->overflow);
            report = now;
            board_touch_report();
        }
        rt_thread_mdelay(16);
    }
failed:
    for (unsigned i = 0; i < METER_BUS_COUNT; ++i)
        if (can_devices[i])
        {
            rt_device_close(can_devices[i]);
            can_devices[i] = NULL;
        }
    lv_aic_deinit();
    lv_deinit();
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
