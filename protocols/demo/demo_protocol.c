#include "protocols/demo/demo_protocol.h"
#include "generated/demo_catalog.h"
#include "protocols/common/meter_frame_router.h"
/* 本合成协议的所有 16 位字段都是小端，s16 按二进制补码解释。 */
static unsigned u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static int s16(const uint8_t *p)
{
    unsigned v = u16(p);
    return v >= 32768u ? (int)v - 65536 : (int)v;
}
static bool emit(meter_update_sink_t sink, void *context, const meter_can_frame_t *f, meter_signal_id_t id,
                 float value, bool bad)
{
    meter_update_t u = {id, {value, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, METER_SOURCE_DEMO}};
    return sink(context, &u);
}
/** @brief 解析 Demo 产品的合成 CAN 帧。
 *
 * 只接受 CAN0 上的标准帧且固定 8 字节数据，其他帧返回 false 表示无人认领。
 * 各字段的缩放、单位与合法范围如下；越界值仍然上报，但状态降级为 ERROR：
 * - 0x100：speed 为小端 u16/100，km/h，0–50.00；steering 为小端 s16/100，度，±45.00；
 *   work_hours 为小端 u16/10，h。
 * - 0x101：soc 为 u8，%，0–100；battery_voltage 为小端 u16/100，V，0–100.00；
 *   charging 取 data[3] 的 bit0。
 * - 0x102：height 为小端 u16/1000，m，0–6.000。
 * - 0x103：load 为小端 u16，kg，0–1500。
 * - 0x104：seat、brake、neutral、warning 依次占用 data[0] 的 bit0–bit3；
 *   motor_temp 与 controller_temp 为小端 s16，摄氏度，-40–150。
 *
 * 允许在 CAN 收包线程调用：只做纯解析并通过 sink 转交结果，不阻塞、也不触碰 LVGL 或 UI 状态。
 */
bool meter_demo_decode(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)
{
    if (!meter_frame_valid(f) || !sink || f->extended || f->bus != METER_BUS_CAN0 || f->size != 8)
        return false;
    bool ok = true;
    switch (f->id)
    {
    case 0x100:
        ok &= emit(sink, ctx, f, METER_SPEED, u16(f->data) / 100.0f, u16(f->data) > 5000);
        ok &= emit(sink, ctx, f, METER_STEERING, s16(f->data + 2) / 100.0f,
                   s16(f->data + 2) < -4500 || s16(f->data + 2) > 4500);
        ok &= emit(sink, ctx, f, METER_WORK_HOURS, u16(f->data + 4) / 10.0f, false);
        break;
    case 0x101:
        ok &= emit(sink, ctx, f, METER_SOC, f->data[0], f->data[0] > 100);
        ok &= emit(sink, ctx, f, METER_BATTERY_VOLTAGE, u16(f->data + 1) / 100.0f, u16(f->data + 1) > 10000);
        ok &= emit(sink, ctx, f, METER_CHARGING, (f->data[3] & 1) != 0, false);
        break;
    case 0x102:
        ok &= emit(sink, ctx, f, METER_HEIGHT, u16(f->data) / 1000.0f, u16(f->data) > 6000);
        break;
    case 0x103:
        ok &= emit(sink, ctx, f, METER_LOAD, (float)u16(f->data), u16(f->data) > 1500);
        break;
    case 0x104:
        ok &= emit(sink, ctx, f, METER_SEAT, (f->data[0] & 1) != 0, false);
        ok &= emit(sink, ctx, f, METER_BRAKE, (f->data[0] & 2) != 0, false);
        ok &= emit(sink, ctx, f, METER_NEUTRAL, (f->data[0] & 4) != 0, false);
        ok &= emit(sink, ctx, f, METER_WARNING, (f->data[0] & 8) != 0, false);
        ok &= emit(sink, ctx, f, METER_MOTOR_TEMP, (float)s16(f->data + 1),
                   s16(f->data + 1) > 150 || s16(f->data + 1) < -40);
        ok &= emit(sink, ctx, f, METER_CONTROLLER_TEMP, (float)s16(f->data + 3),
                   s16(f->data + 3) > 150 || s16(f->data + 3) < -40);
        break;
    default:
        return false;
    }
    return ok;
}
