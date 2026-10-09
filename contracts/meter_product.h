#ifndef METER_PRODUCT_H
#define METER_PRODUCT_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "contracts/meter_protocol.h"
#include "contracts/meter_update_view.h"
#include "contracts/meter_execution.h"
#include "contracts/meter_periodic.h"
#include <stddef.h>
typedef bool (*meter_decode_fn_t)(const meter_can_frame_t *frame, meter_update_sink_t sink, void *context);
typedef struct
{
    meter_frame_route_owner_t owner;
    meter_decode_fn_t decode;
    const meter_protocol_adapter_t *adapter;
} meter_protocol_binding_t;
typedef struct
{
    const meter_protocol_binding_t *bindings;
    size_t count;
} meter_protocol_profile_t;
/** @brief Product 只选择业务命令的目标协议 owner，不触碰 TX；Runtime 统一调用 Adapter。
 *
 * 返回 true 且写 *owner_out 表示可路由；返回 false 表示未知命令。
 * Router 不得直接调用 tx 发送，发送只能由目标 Adapter 的 command() 通过
 * services->tx 完成，避免绕过协议适配层。
 */
typedef bool (*meter_command_route_fn_t)(void *context, const meter_command_t *command,
                                         meter_frame_route_owner_t *owner_out);
typedef struct
{
    const meter_frame_route_t *entries;
    size_t count;
} meter_route_profile_t;
typedef struct
{
    bool height, weighing, parameter_write, maintenance, language_selection;
} meter_capability_profile_t;
typedef struct
{
    const char *language;
    const char *title;
} meter_locale_profile_t;
typedef struct
{
    const char *manifest;
} meter_resource_profile_t;
typedef struct
{
    /* Static Product policy, not credentials or an expiring meter_authorization_t grant. */
    bool local_settings;
    bool vehicle_control;
} meter_auth_profile_t;
typedef struct
{
    void *(*create)(void *parent, const meter_ui_actions_t *actions);
    /** @brief `elapsed_ms` 是自 UI 创建起累计的运行毫秒（单调时基，允许 32 位回绕），
        闪烁相位与开机停留都按它算；传帧间隔会让这类计时永远走不完。 */
    void (*present)(void *ui, const meter_snapshot_t *snapshot, uint32_t elapsed_ms);
    void (*destroy)(void *ui);
    /** @brief 可选只读升级展示，由 UI owner 调用，不触发下载或激活。 */
    void (*present_update)(void *ui, const meter_update_view_t *view, meter_language_t language);
} meter_ui_factory_t;
/** @brief Product 本机设置记录策略；未绑定时平台不启用持久化。 */
typedef struct
{
    bool enabled;
    uint16_t record_type, product_namespace, schema;
    uint32_t debounce_ms, max_delay_ms;
} meter_storage_profile_t;
typedef struct
{
    const char *id;
    const meter_capability_profile_t *capabilities;
    const meter_protocol_profile_t *protocols;
    const meter_route_profile_t *routes;
    const meter_ui_factory_t *ui;
    const meter_resource_profile_t *resources;
    const meter_locale_profile_t *locale;
    const meter_auth_profile_t *auth;
    const meter_catalog_t *catalog;
    void (*evaluate)(meter_snapshot_t *snapshot);
    meter_command_route_fn_t command_route;
    void *command_route_context;
    const meter_storage_profile_t *storage;
    /** @brief 可选升级身份和容量策略；未绑定时不启用。 */
    const struct meter_update_policy *update;
    /** @brief App 提供快照，Product 决定维护模式与业务准入。 */
    bool (*update_admission)(const meter_snapshot_t *, bool maintenance);
    /** @brief Product 选择升级维护时暂停正常协议业务与仪表刷新。 */
    bool update_exclusive;
    /** @brief 可选模式策略，App 调用并复制结果到其他 owner；不得操作设备。 */
    meter_mode_policy_t (*mode_policy)(meter_mode_t mode);
    /** @brief 不可变周期配置；配置及数组覆盖整个运行期。 */
    const meter_periodic_frame_t *periodic;
    size_t periodic_count;
    /** @brief App 采样回调和语义数量；板级预算提供独立存储。 */
    meter_tx_sample_fn_t tx_sample;
    size_t tx_value_count;
    /** @brief App 消费协议事件并执行产品 workflow；禁止解码 CAN 或操作设备。 */
    void (*on_event)(const meter_protocol_event_t *event);
    /** @brief App 在 generation 切换时先复位产品工作流，再接收新事件。 */
    void (*app_reset)(uint32_t generation);
    /** @brief App 有界 runnable；只能通过语义命令端口访问协议，不阻塞。 */
    void (*app_run)(uint32_t now_ms, const meter_command_port_t *commands);
    /** @brief 可选命令完成条件；纯函数，返回 APPLIED/TX_COMPLETED/REMOTE_CONFIRMED，默认要求远端确认。 */
    meter_command_stage_t (*command_completion)(const meter_command_t *command);
    /** @brief App owner 内的可选设置入口：普通动作返回准入，PRODUCT 动作返回已处理。
     * snapshot 仅在回调内可修改，发生变化须递增 revision；不得保留指针、阻塞或操作设备。
     * 未经回调处理的 PRODUCT 动作拒绝。队列接纳与 RAM 应用都不代表 NVM 已持久化。 */
    bool (*local_action)(meter_snapshot_t *snapshot, const meter_action_t *action, uint32_t now_ms);
    /** @brief 可选 Product 默认设置；NULL 保持兼容默认，不代表工厂恢复策略。 */
    const meter_initial_settings_t *initial_settings;
    /** @brief App owner 的本机周期计算，信号超时处理后、evaluate/发布前调用。
     * now_ms 是单调毫秒；维护/断线仍调用，停止阶段不调用。
     * snapshot 仅在回调内借用；变更须递增 revision，不得阻塞或执行设备 I/O。
     * 原生端在 NVM 恢复后调用，参数变化进入既有去抖保存；RAM 更新不是持久化确认。 */
    void (*app_tick)(meter_snapshot_t *snapshot, uint32_t now_ms);
    /** @brief 显式允许同一 schema 的旧 MSP3 缺少新增参数；已有 ID/含义/范围不得改变。 */
    bool allow_parameter_extension;
} meter_product_t;
const meter_product_t *meter_product_get(void);
#endif
