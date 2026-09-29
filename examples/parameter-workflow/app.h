#ifndef REFERENCE_PARAMETER_APP_H
#define REFERENCE_PARAMETER_APP_H
#include "contracts/meter_profile.h"
#include "examples/parameter-workflow/ui/presentation.h"
#include "runtime/meter_parameters.h"
/* 保留参考示例的源代码兼容名称，复制消息契约由公共头文件统一定义。 */
typedef meter_parameter_exchange_t reference_parameter_port_t;
typedef struct
{
    meter_parameters_t parameters;
    meter_authorization_t authorization;
    meter_parameter_policy_t policy;
    reference_parameter_view_t view;
    uint32_t generation;
    bool active;
    bool profile_ready;
} reference_parameter_app_t;
/* 本示例在 app.c 固定定义字段到参数键的映射；调用方提供的合成目录
 * 决定这些键的确认状态、范围及权限。接口仅由 App 串行调用，要求对象
 * 已初始化且非空，端口回调有效。输入输出不得与 App 状态重叠，禁止复制
 * 活动实例，不新增线程或锁。UI 通过已有复制消息通道提交意图，不得持有
 * 此对象。session 和配置代数必须非零，其生命周期由调用方管理。 */
bool reference_parameter_app_init(reference_parameter_app_t *app, const meter_parameter_definition_t *catalog,
                                  size_t count, uint32_t session, uint32_t generation,
                                  meter_parameter_policy_t policy);
meter_parameter_admission_t reference_parameter_app_submit(reference_parameter_app_t *app,
                                                           const reference_parameter_intent_t *intent,
                                                           uint32_t now_ms);
void reference_parameter_app_step(reference_parameter_app_t *app, const reference_parameter_port_t *port,
                                  uint32_t now_ms);
bool reference_parameter_app_cancel(reference_parameter_app_t *app, uint64_t token, meter_request_id_t id);
bool reference_parameter_app_acknowledge(reference_parameter_app_t *app, uint64_t token,
                                         meter_request_id_t id);
/* App 将展示值复制到发布通道；UI 接收独立副本，不持有状态别名。 */
void reference_parameter_app_present(const reference_parameter_app_t *app, reference_parameter_view_t *out);
/* 仅为参考策略：配置替换时取消调用者关注并撤销授权。原始令牌、代数及
 * 结果保留到确认领取，后端仍负责排空。本示例不决定生产安全策略；拒绝
 * 回退或复用代数。 */
bool reference_parameter_app_profile(reference_parameter_app_t *app, const meter_profile_t *profile);
#endif
