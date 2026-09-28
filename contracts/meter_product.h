#ifndef METER_PRODUCT_H
#define METER_PRODUCT_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "contracts/meter_protocol.h"
#include "contracts/meter_update_view.h"
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
    bool local_settings;
    bool vehicle_control;
} meter_auth_profile_t;
typedef struct
{
    void *(*create)(void *parent, const meter_ui_actions_t *actions);
    void (*present)(void *ui, const meter_snapshot_t *snapshot, uint32_t elapsed_ms);
    void (*destroy)(void *ui);
    /** @brief 可选只读升级展示，由 App/UI owner 调用，不触发下载或激活。 */
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
} meter_product_t;
const meter_product_t *meter_product_get(void);
#endif
