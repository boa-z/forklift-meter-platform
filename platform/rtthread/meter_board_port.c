#define LOG_TAG "meter.board"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_board_port.h"
#include "lv_aic_display.h"
#include "lv_aic_indev.h"
#include "lvgl_aic.h"
#include <lvgl.h>
#include <mpp_fb.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>
#include <ulog.h>
#ifdef METER_ENABLE_CAN_UPDATE
#include "platform/rtthread/meter_update_port.h"
#endif
static rt_device_t can_devices[METER_BUS_COUNT];
static unsigned next_bus;
static uint32_t board_now(void *ctx)
{
    (void)ctx;
    return (uint32_t)rt_tick_get_millisecond();
}
/* 此 Demo 独占显示层；包含视频的产品必须提供自己的层策略。 */
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
static bool board_can_open(void *ctx, meter_bus_role_t bus)
{
    meter_diagnostics_t *diag = ctx;
    static const char *const names[] = {"can0", "can1"};
    if ((unsigned)bus >= METER_BUS_COUNT)
        return false;
    meter_diag_can_t *d = &diag->data.can[bus];
    d->available = true;
    d->board_available = true;
    rt_device_t dev = rt_device_find(names[bus]);
    unsigned flags = RT_DEVICE_FLAG_INT_RX;
#ifdef METER_ENABLE_CAN_UPDATE
    flags |= RT_DEVICE_FLAG_INT_TX;
#endif
    if (!dev || rt_device_open(dev, flags) != RT_EOK)
    {
        meter_diagnostics_can(diag, bus, METER_CAN_RX_ERROR, board_now(NULL));
        ulog_e("meter.can", "cannot open %s", names[bus]);
        return false;
    }
    /* 公开测试总线为 500 kbit/s，只接收，不发送车辆命令。 */
    if (rt_device_control(dev, RT_CAN_CMD_SET_BAUD, (void *)CAN500kBaud) != RT_EOK ||
        rt_device_control(dev, RT_DEVICE_CTRL_SET_INT, NULL) != RT_EOK)
    {
        meter_diagnostics_can(diag, bus, METER_CAN_RX_ERROR, board_now(NULL));
        ulog_e("meter.can", "cannot configure %s", names[bus]);
        rt_device_close(dev);
        return false;
    }
    can_devices[bus] = dev;
    d->open = true;
    d->bitrate = 500000;
    ulog_i("meter.can", "%s opened at 500000 bit/s (public Demo RX)", names[bus]);
    return true;
}
#ifdef METER_ENABLE_CAN_UPDATE
bool meter_board_can_raw_read(void *ctx, meter_can_frame_t *frame)
#else
static bool board_can_read(void *ctx, meter_can_frame_t *frame)
#endif
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

#ifdef METER_ENABLE_CAN_UPDATE
static bool board_can_read(void *ctx, meter_can_frame_t *frame)
{
    return meter_board_update_started() ? meter_board_update_read(frame)
                                        : meter_board_can_raw_read(ctx, frame);
}
bool meter_board_can_send(const meter_can_frame_t *frame)
{
    if (!frame || (unsigned)frame->bus >= METER_BUS_COUNT || !can_devices[frame->bus] || frame->size > 8u)
        return false;
    struct rt_can_msg message = {0};
    message.id = frame->id;
    message.len = frame->size;
    message.ide = frame->extended;
    message.rtr = frame->remote;
    message.hdr = -1;
    memcpy(message.data, frame->data, frame->size);
    return rt_device_write(can_devices[frame->bus], 0, &message, sizeof(message)) == sizeof(message);
}
#endif
meter_rtthread_board_port_t meter_board_port(meter_diagnostics_t *diag)
{
    return (meter_rtthread_board_port_t){.display_init = board_display,
                                         .touch_init = board_touch,
                                         .can_open = board_can_open,
                                         .can_read = board_can_read,
                                         .context = diag,
                                         .now_ms = board_now};
}
void meter_board_diagnostics(meter_diagnostics_t *diag)
{
    if (!diag)
        return;
    diag->data.ui.flush_available = true;
    diag->data.ui.flush_count = lv_aic_display_flush_count_get();
    lv_aic_touch_diagnostics_t d;
    if (lv_aic_indev_get_diagnostics(lv_aic_get_pointer_indev(), &d) == LV_AIC_OK)
    {
        diag->data.touch = (meter_diag_touch_t){.available = true,
                                                .range_x = d.range_x,
                                                .range_y = d.range_y,
                                                .x = d.x,
                                                .y = d.y,
                                                .state = d.state,
                                                .irq = d.irqs,
                                                .reads = d.reads,
                                                .events = d.events,
                                                .delivered = d.deliveries,
                                                .recovered = d.recovered,
                                                .empty_reads = d.empty_reads,
                                                .invalid_reads = d.invalid_reads};
    }
}
void meter_board_close(meter_diagnostics_t *diag)
{
    for (unsigned i = 0; i < METER_BUS_COUNT; ++i)
        if (can_devices[i])
        {
            rt_device_close(can_devices[i]);
            can_devices[i] = NULL;
            if (diag)
                diag->data.can[i].open = false;
        }
    lv_aic_deinit();
    if (diag)
    {
        diag->data.touch.available = false;
        diag->data.ui.available = false;
        diag->data.ui.flush_available = false;
    }
}
