#define LOG_TAG "meter.board"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_board_color.h"
#include "platform/rtthread/meter_board_testcard.h"
#include "lv_aic_display.h"
#include "lv_aic_indev.h"
#include "lvgl_aic.h"
#include <lvgl.h>
#include <aic_core.h>
#include <aic_time.h>
#include <mpp_fb.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>
#include <ulog.h>
#ifndef AIC_FORKLIFT_CAN0_DEVICE
#define AIC_FORKLIFT_CAN0_DEVICE "can0"
#endif
#ifndef AIC_FORKLIFT_CAN1_DEVICE
#define AIC_FORKLIFT_CAN1_DEVICE "can1"
#endif
static const char *const can_names[METER_BUS_COUNT] = {AIC_FORKLIFT_CAN0_DEVICE, AIC_FORKLIFT_CAN1_DEVICE};
static rt_device_t can_devices[METER_BUS_COUNT];
static unsigned next_bus;
static struct rt_semaphore *rx_wake;
static rt_err_t rx_indicate(rt_device_t device, rt_size_t size)
{
    (void)device;
    (void)size;
    return rx_wake ? rt_sem_release(rx_wake) : -RT_ERROR;
}
void meter_board_can_wake(struct rt_semaphore *wake)
{
    rx_wake = wake;
}
uint32_t meter_board_now_ms(void)
{
    return (uint32_t)aic_get_time_ms();
}
#ifdef AIC_FORKLIFT_DISPLAY_EXCLUSIVE
/* UI 独占显示层：关掉视频层与额外 UI 矩形，全局 alpha 255，关闭色键。用视频层的产品在 Kconfig 关掉此项。 */
static bool display_takeover(void)
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
#endif
bool meter_board_display_init(void)
{
#ifdef AIC_FORKLIFT_DISPLAY_EXCLUSIVE
    if (!display_takeover())
        return false;
#endif
    int result = lv_aic_init();
    if (result != LV_AIC_OK)
    {
        LOG_E("display/touch initialization failed: %d", result);
        return false;
    }
    if (!lv_aic_get_pointer_indev())
        LOG_W("no pointer input device; UI runs without touch");
    meter_board_color_init();
    meter_board_testcard_init();
    return true;
}
void meter_board_display_close(meter_diagnostics_t *diag)
{
    lv_aic_deinit();
    if (diag)
    {
        diag->data.touch.available = false;
        diag->data.ui.available = false;
        diag->data.ui.flush_available = false;
    }
}
bool meter_board_can_open(meter_bus_role_t bus, uint32_t bitrate, meter_diagnostics_t *diag)
{
    if ((unsigned)bus >= METER_BUS_COUNT)
        return false;
    meter_diag_can_t *d = diag ? &diag->data.can[bus] : NULL;
    if (d)
        d->available = d->board_available = true;
    rt_device_t dev = rt_device_find(can_names[bus]);
    if (!dev || rt_device_open(dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX) != RT_EOK)
    {
        meter_diagnostics_can(diag, bus, METER_CAN_RX_ERROR, meter_board_now_ms());
        LOG_E("cannot open %s", can_names[bus]);
        return false;
    }
    if (rt_device_control(dev, RT_CAN_CMD_SET_BAUD, (void *)(uintptr_t)bitrate) != RT_EOK ||
        rt_device_control(dev, RT_DEVICE_CTRL_SET_INT, NULL) != RT_EOK ||
        rt_device_set_rx_indicate(dev, rx_indicate) != RT_EOK)
    {
        meter_diagnostics_can(diag, bus, METER_CAN_RX_ERROR, meter_board_now_ms());
        LOG_E("cannot configure %s", can_names[bus]);
        (void)rt_device_close(dev);
        return false;
    }
    can_devices[bus] = dev;
    if (d)
    {
        d->open = true;
        d->bitrate = bitrate;
    }
    LOG_I("bus %u on %s opened at %lu bit/s", (unsigned)bus, can_names[bus], (unsigned long)bitrate);
    return true;
}
bool meter_board_can_read(meter_can_frame_t *frame)
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
        frame->timestamp_ms = meter_board_now_ms();
        memcpy(frame->data, msg.data, msg.len <= sizeof(frame->data) ? msg.len : sizeof(frame->data));
        return true;
    }
    return false;
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
bool meter_board_can_stop(meter_bus_role_t bus)
{
    return (unsigned)bus < METER_BUS_COUNT && can_devices[bus] &&
           rt_device_control(can_devices[bus], RT_CAN_CMD_SET_MODE,
                             (void *)(uintptr_t)RT_CAN_MODE_LISTEN) == RT_EOK;
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
void meter_board_can_diagnostics(meter_diagnostics_t *diag)
{
    if (!diag)
        return;
    for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
    {
        struct rt_can_status status = {0};
        if (can_devices[bus] && rt_device_control(can_devices[bus], RT_CAN_CMD_GET_STATUS, &status) == RT_EOK)
        {
            diag->data.can[bus].rx_drop = status.dropedrcvpkg;
            diag->data.can[bus].rx_error = status.rcverrcnt;
        }
    }
}
