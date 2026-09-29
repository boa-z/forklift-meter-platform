#ifndef METER_CALIBRATION_H
#define METER_CALIBRATION_H
#include "contracts/meter_calibration.h"
#include "runtime/meter_parameters.h"
typedef struct
{
    meter_parameters_t parameters;
    meter_calibration_definition_t definition;
    meter_calibration_view_t view;
    meter_request_id_t request;
    uint32_t captured_ms;
    bool dispatched;
} meter_calibration_t;
/* 由 App 串行调用，仅初始化一次，禁止复制活动服务。输入输出不得与
 * 服务状态重叠。初始化成功后，step/take/reply/present/acknowledge 要求
 * 服务已初始化且指针非空；Product 必须传入当前一致的配置。 */
bool meter_calibration_init(meter_calibration_t *service, const meter_parameter_definition_t *catalog,
                            size_t count, uint32_t session);
/* 返回 false：参数无效或仍有保留流程，状态不变。返回 true：流程已接纳，
 * 也可能立即以明确的拒绝结果进入 DONE；此时尚未线上派发。
 * measurement_generation 证明测量来自对应配置，Product 不得给旧控制器
 * 缓存值套用新代数；snapshot 必须一致。token 是不复用的面板或流程
 * 生命周期身份，不是授权凭据。 */
bool meter_calibration_begin(meter_calibration_t *service, const meter_calibration_definition_t *definition,
                             const meter_snapshot_t *snapshot, uint32_t measurement_generation,
                             uint64_t token, bool prerequisites, const meter_authorization_t *authorization,
                             uint32_t now_ms);
void meter_calibration_step(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms);
/* 仅在后端拥有独立的排空/隔离额度时调用；复制一条请求。 */
bool meter_calibration_take(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms,
                            meter_parameter_work_t *work);
bool meter_calibration_reply(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                             const meter_authorization_t *authorization, uint32_t now_ms,
                             const meter_parameter_reply_t *reply);
void meter_calibration_present(const meter_calibration_t *service, meter_calibration_view_t *out);
bool meter_calibration_acknowledge(meter_calibration_t *service, uint32_t generation, uint64_t token);
#endif
