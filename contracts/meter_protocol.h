#ifndef METER_PROTOCOL_H
#define METER_PROTOCOL_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
/** @brief 同步提交 Domain 更新；sink context 独立于协议状态，返回是否接受更新。 */
typedef bool (*meter_update_sink_t)(void *context, const meter_update_t *update);
/** @brief 平台无关的 CAN 发送端口；实现负责线程安全、队列和硬件时序。 */
typedef struct
{
    bool (*send)(void *context, const meter_can_frame_t *frame);
    void *context;
} meter_can_tx_port_t;
/** @brief 车辆命令，与 UI 的本地设置动作完全分离。单位由产品协议定义。 */
typedef struct
{
    uint16_t id;
    float value;
    uint32_t argument;
} meter_command_t;
/** @brief 有状态协议适配器的生命周期回调；回调运行在协议上下文，不得触碰 LVGL。 */
typedef struct
{
    void *context;
    bool (*on_frame)(void *context, const meter_can_frame_t *frame, meter_update_sink_t sink,
                     void *sink_context);
    bool (*process)(void *context, uint32_t now_ms, meter_update_sink_t sink, void *sink_context);
    bool (*command)(void *context, const meter_command_t *command, const meter_can_tx_port_t *tx);
    void (*reset)(void *context);
} meter_protocol_adapter_t;
#endif
