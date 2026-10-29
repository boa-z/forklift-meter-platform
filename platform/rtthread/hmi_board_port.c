#define LOG_TAG "hmi.board"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/hmi_board_port.h"
#include "platform/rtthread/hmi_board_color.h"
#include <lvgl.h>
#include <aic_core.h>
#include <aic_time.h>
#include <mpp_fb.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>
#include <ulog.h>
#ifndef AIC_HMI_CAN0_DEVICE
#define AIC_HMI_CAN0_DEVICE "can0"
#endif
#ifndef AIC_HMI_CAN1_DEVICE
#define AIC_HMI_CAN1_DEVICE "can1"
#endif
static const char *const can_names[HMI_BUS_COUNT] = {AIC_HMI_CAN0_DEVICE, AIC_HMI_CAN1_DEVICE};
static rt_device_t can_devices[HMI_BUS_COUNT];
static unsigned next_bus;
static struct rt_semaphore *rx_wake;
static rt_err_t rx_indicate(rt_device_t device, rt_size_t size)
{
    (void)device;
    (void)size;
    return rx_wake ? rt_sem_release(rx_wake) : -RT_ERROR;
}
void hmi_board_can_wake(struct rt_semaphore *wake)
{
    rx_wake = wake;
}
uint32_t hmi_board_now_ms(void)
{
    return (uint32_t)aic_get_time_ms();
}
#ifdef AIC_HMI_DISPLAY_EXCLUSIVE
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
static uint32_t flush_count;
static uint32_t touch_events;
static void flush_event(lv_event_t *event)
{
    (void)event;
    ++flush_count;
}
static void touch_event(lv_event_t *event)
{
    (void)event;
    ++touch_events;
}
static lv_indev_t *pointer_indev(void)
{
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev))
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
            return indev;
    return NULL;
}
bool hmi_board_display_init(void)
{
    /* SDK 的 LVGL 线程已经建好显示与输入设备（lv_port_disp_init / lv_port_indev_init）。 */
    lv_display_t *display = lv_display_get_default();
    if (!display)
    {
        LOG_E("no LVGL display");
        return false;
    }
#ifdef AIC_HMI_DISPLAY_EXCLUSIVE
    if (!display_takeover())
        return false;
#endif
    lv_display_add_event_cb(display, flush_event, LV_EVENT_FLUSH_FINISH, NULL);
    lv_indev_t *pointer = pointer_indev();
    if (pointer)
        lv_indev_add_event_cb(pointer, touch_event, LV_EVENT_PRESSED, NULL);
    else
        LOG_W("no pointer input device; UI runs without touch");
    hmi_board_color_init();
    return true;
}
void hmi_board_display_close(hmi_diagnostics_t *diag)
{
    if (diag)
    {
        diag->data.touch.available = false;
        diag->data.ui.available = false;
        diag->data.ui.flush_available = false;
    }
}
bool hmi_board_can_open(hmi_bus_role_t bus, uint32_t bitrate, hmi_diagnostics_t *diag)
{
    if ((unsigned)bus >= HMI_BUS_COUNT)
        return false;
    hmi_diag_can_t *d = diag ? &diag->data.can[bus] : NULL;
    if (d)
        d->available = d->board_available = true;
    rt_device_t dev = rt_device_find(can_names[bus]);
    if (!dev || rt_device_open(dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX) != RT_EOK)
    {
        hmi_diagnostics_can(diag, bus, HMI_CAN_RX_ERROR, hmi_board_now_ms());
        LOG_E("cannot open %s", can_names[bus]);
        return false;
    }
    if (rt_device_control(dev, RT_CAN_CMD_SET_BAUD, (void *)(uintptr_t)bitrate) != RT_EOK ||
        rt_device_control(dev, RT_DEVICE_CTRL_SET_INT, NULL) != RT_EOK ||
        rt_device_set_rx_indicate(dev, rx_indicate) != RT_EOK)
    {
        hmi_diagnostics_can(diag, bus, HMI_CAN_RX_ERROR, hmi_board_now_ms());
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
bool hmi_board_can_read(hmi_can_frame_t *frame)
{
    for (unsigned i = 0; i < HMI_BUS_COUNT; ++i)
    {
        unsigned bus = next_bus;
        next_bus = (next_bus + 1U) % HMI_BUS_COUNT;
        if (!can_devices[bus])
            continue;
        struct rt_can_msg msg = {0};
        msg.hdr = -1;
        if (rt_device_read(can_devices[bus], 0, &msg, sizeof(msg)) != sizeof(msg))
            continue;
        memset(frame, 0, sizeof(*frame));
        frame->bus = (hmi_bus_role_t)bus;
        frame->id = msg.id;
        frame->extended = msg.ide != 0;
        frame->remote = msg.rtr != 0;
        frame->size = msg.len;
        frame->timestamp_ms = hmi_board_now_ms();
        memcpy(frame->data, msg.data, msg.len <= sizeof(frame->data) ? msg.len : sizeof(frame->data));
        return true;
    }
    return false;
}
bool hmi_board_can_send(const hmi_can_frame_t *frame)
{
    if (!frame || (unsigned)frame->bus >= HMI_BUS_COUNT || !can_devices[frame->bus] || frame->size > 8u)
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
bool hmi_board_can_stop(hmi_bus_role_t bus)
{
    return (unsigned)bus < HMI_BUS_COUNT && can_devices[bus] &&
           rt_device_control(can_devices[bus], RT_CAN_CMD_SET_MODE,
                             (void *)(uintptr_t)RT_CAN_MODE_LISTEN) == RT_EOK;
}
void hmi_board_diagnostics(hmi_diagnostics_t *diag)
{
    if (!diag)
        return;
    diag->data.ui.flush_available = true;
    diag->data.ui.flush_count = flush_count;
    lv_indev_t *pointer = pointer_indev();
    lv_display_t *display = lv_display_get_default();
    if (pointer && display)
    {
        /* SDK 的输入驱动不暴露中断、读取与恢复计数，这些字段保持 0；这里只报告 LVGL 看得到的。 */
        lv_point_t point;
        lv_indev_get_point(pointer, &point);
        diag->data.touch = (hmi_diag_touch_t){.available = true,
                                                .range_x = lv_display_get_horizontal_resolution(display),
                                                .range_y = lv_display_get_vertical_resolution(display),
                                                .x = point.x,
                                                .y = point.y,
                                                .state = lv_indev_get_state(pointer),
                                                .events = touch_events};
    }
}
void hmi_board_can_diagnostics(hmi_diagnostics_t *diag)
{
    if (!diag)
        return;
    for (unsigned bus = 0u; bus < HMI_BUS_COUNT; ++bus)
    {
        struct rt_can_status status = {0};
        if (can_devices[bus] && rt_device_control(can_devices[bus], RT_CAN_CMD_GET_STATUS, &status) == RT_EOK)
        {
            diag->data.can[bus].rx_drop = status.dropedrcvpkg;
            diag->data.can[bus].rx_error = status.rcverrcnt;
        }
    }
}
