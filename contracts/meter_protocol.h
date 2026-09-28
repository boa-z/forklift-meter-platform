#ifndef METER_PROTOCOL_H
#define METER_PROTOCOL_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "contracts/meter_request.h"
/** @brief 请求进度互不等价；TX_COMPLETED 仅表示驱动完成，REMOTE_CONFIRMED 必须来自协议响应。 */
typedef enum
{
    METER_COMMAND_QUEUED, METER_COMMAND_APPLIED, METER_COMMAND_TX_COMPLETED,
    METER_COMMAND_REMOTE_CONFIRMED, METER_COMMAND_FAILED, METER_COMMAND_CANCELLED
} meter_command_stage_t;
/** @brief 按请求身份复制进度；Protocol owner 调用，回调不得阻塞或写 Core。 */
typedef void (*meter_command_progress_fn_t)(void *context, meter_request_id_t id, meter_command_stage_t stage);
struct meter_diagnostics;
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
/** @brief App 请求端口；仅传递复制值与身份，不暴露协议对象或 CAN 字节。 */
typedef struct
{
    void *context;
    meter_request_admission_t (*submit)(void *, const meter_command_t *, uint32_t, meter_request_id_t *);
    bool (*result)(void *, meter_request_id_t, meter_command_stage_t *, bool acknowledge);
} meter_command_port_t;
/** @brief 协议层瞬时事件；事件不写入持续 signal 槽位。 */
typedef struct
{
    uint16_t id;
    uint32_t timestamp_ms;
    uint32_t argument;
} meter_protocol_event_t;
typedef bool (*meter_protocol_event_sink_t)(void *context, const meter_protocol_event_t *event);
/**
 * @brief adapter 所需的统一协议服务，回调运行在 protocol/app owner 上下文。
 *
 * update/update_context 写入持续 Domain 信号；event/event_context 投递瞬时事件，
 * 两者上下文相互独立，adapter 不得混用。tx 为板级发送端口，仅在回调期间有效。
 * 非阻塞事务（如 SDO）由 adapter 或独立静态 manager 在产品侧管理，
 * 不再由通用 Runtime 保存“整个系统唯一的 transaction”。
 */
typedef struct
{
    meter_update_sink_t update;
    void *update_context;
    meter_protocol_event_sink_t event;
    void *event_context;
    const meter_can_tx_port_t *tx;
    struct meter_diagnostics *diagnostics; /**< 可选观测实例，不拥有其生命周期。 */
    meter_request_id_t request; /**< 当前 command 调用身份，process/on_frame 时为零。 */
    meter_command_progress_fn_t progress; /**< 异步 Adapter 保存身份，不保存 services 指针。 */
    void *progress_context;
} meter_protocol_services_t;
/**
 * @brief 有状态协议适配器的唯一生命周期回调；回调运行在协议上下文，不得触碰 LVGL。
 *
 * 历史遗留的 on_frame/process/command 双接口（sink+ctx 与 services 并存）已删除，
 * 所有产品统一使用本 services 接口。无状态 DBC 解码仍可使用 meter_decode_fn_t。
 */
typedef struct
{
    void *context;
    bool (*on_frame)(void *context, const meter_can_frame_t *frame,
                     const meter_protocol_services_t *services);
    bool (*process)(void *context, uint32_t now_ms, const meter_protocol_services_t *services);
    bool (*command)(void *context, const meter_command_t *command, const meter_protocol_services_t *services);
    void (*reset)(void *context);
    /** @brief Protocol 取消对应请求；必须停止本机重试，不能保证已经发送的远端写被撤回。 */
    void (*cancel)(void *context, meter_request_id_t request);
} meter_protocol_adapter_t;
#endif
