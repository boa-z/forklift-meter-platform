#ifndef METER_PROTOCOL_H
#define METER_PROTOCOL_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
struct meter_diagnostics;
/** @brief 同步提交 Domain 更新；返回是否接受更新。 */
typedef bool (*meter_update_sink_t)(void *context, const meter_update_t *update);
/** @brief 平台无关的 CAN 发送端口；true 只表示帧已进入该总线的发送队列，不代表已上线或被应答。 */
typedef struct
{
    bool (*send)(void *context, const meter_can_frame_t *frame);
    void *context;
} meter_can_tx_port_t;
/** @brief 车辆命令，与 UI 的本地设置动作完全分离。单位与含义由产品协议定义。 */
typedef struct
{
    uint16_t id;
    float value;
    uint32_t argument;
} meter_command_t;
/**
 * @brief App 的命令端口。
 *
 * App 与协议适配器运行在同一个运行时线程：send 同步按 Product 的 command_route 找到目标适配器并调用其
 * command()，返回适配器是否接受。需要远端应答、超时或重试的事务由适配器自己在 on_frame/process 里推进，
 * 结果以信号写入 Domain。
 */
typedef struct
{
    void *context;
    bool (*send)(void *context, const meter_command_t *command);
} meter_command_port_t;
/**
 * @brief 适配器可用的协议服务，只在回调期间有效。
 *
 * update 写入持续 Domain 信号；tx 为板级发送端口；diagnostics 为可选观测实例。
 */
typedef struct
{
    meter_update_sink_t update;
    void *update_context;
    const meter_can_tx_port_t *tx;
    struct meter_diagnostics *diagnostics;
} meter_protocol_services_t;
/**
 * @brief 有状态协议适配器（SDO 客户端/服务端、CANopenNode 包装、私有握手等）。
 *
 * 回调都在运行时线程串行执行，不得阻塞、不得触碰 LVGL。process 每轮调用一次（无帧时也调用），
 * 可用于推进超时或轮询非 CAN 的 I/O；reset 在会话代次切换时调用。无状态的 DBC 解码使用 meter_decode_fn_t。
 */
typedef struct
{
    void *context;
    bool (*on_frame)(void *context, const meter_can_frame_t *frame, const meter_protocol_services_t *services);
    bool (*process)(void *context, uint32_t now_ms, const meter_protocol_services_t *services);
    bool (*command)(void *context, const meter_command_t *command, const meter_protocol_services_t *services);
    void (*reset)(void *context);
} meter_protocol_adapter_t;
#endif
