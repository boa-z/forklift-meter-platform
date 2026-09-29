#include "services/settings_app.h"
#include "application/presentation.h"
#include "runtime/meter_authorization.h"
#include "runtime/meter_parameters.h"
#include <math.h>

/* 公开演示凭据只证明授权流程，不作为真实车辆身份或安全策略。 */
enum { USER_PERMISSION = 1, ADMIN_PERMISSION = 2, GRANT_MS = 60000 };
static const meter_parameter_definition_t remote_catalog[DEMO_REMOTE_COUNT] = {
    {{1, 1}, true, true, true, true, 5, 50, 0, ADMIN_PERMISSION},
    {{1, 2}, true, true, true, true, 0, 10, 0, ADMIN_PERMISSION},
    {{2, 1}, true, true, true, true, 0, 8, 0, ADMIN_PERMISSION},
    {{2, 2}, true, true, true, true, 0, 100, 0, ADMIN_PERMISSION}
};
/* App 单一所有者；既有快照发布复制下方结果，UI 不读取这些可变对象。 */
static meter_authorization_t grant;
static meter_parameters_t parameters;
static meter_request_id_t pending;
static bool initialized, pending_valid;
static uint32_t app_now;
static demo_feedback_t auth_feedback, parameter_feedback;
static unsigned admin_values[DEMO_ADMIN_COUNT];
static float remote_values[DEMO_REMOTE_COUNT] = {25, 3, 4, 30};

static bool initialize(void)
{
    if (!initialized)
        initialized = meter_parameters_init(&parameters, remote_catalog, DEMO_REMOTE_COUNT, 1);
    return initialized;
}
void demo_settings_reset(uint32_t generation)
{
    (void)generation;
    meter_authorization_revoke(&grant);
    auth_feedback = DEMO_FEEDBACK_IDLE;
    /* 不重建存活的 ledger；模式切换撤销授权，由下次 tick 收敛旧事务。 */
}
bool demo_settings_action(meter_snapshot_t *snapshot, const meter_action_t *action, uint32_t now_ms)
{
    if (!snapshot || !action || !isfinite(action->value) || !initialize())
        return false;
    app_now = now_ms;
    if (action->kind != METER_ACTION_PRODUCT)
        return true;
    if (action->id == DEMO_INTENT_LOGOUT)
    {
        meter_authorization_revoke(&grant);
        auth_feedback = DEMO_FEEDBACK_IDLE;
        return true;
    }
    if (action->id == DEMO_INTENT_USER_LOGIN || action->id == DEMO_INTENT_ADMIN_LOGIN)
    {
        bool admin = action->id == DEMO_INTENT_ADMIN_LOGIN;
        bool accepted = action->value == (admin ? 5312.0f : 1234.0f);
        meter_authorization_revoke(&grant);
        if (accepted)
            accepted = meter_authorization_grant(&grant, admin ? USER_PERMISSION | ADMIN_PERMISSION : USER_PERMISSION,
                                                  now_ms, GRANT_MS);
        auth_feedback = accepted ? DEMO_FEEDBACK_APPLIED : DEMO_FEEDBACK_DENIED;
        return accepted;
    }
    if (!meter_authorization_allows(&grant, ADMIN_PERMISSION, now_ms))
    {
        auth_feedback = DEMO_FEEDBACK_DENIED;
        parameter_feedback = DEMO_FEEDBACK_DENIED;
        return false;
    }
    if (action->id >= DEMO_INTENT_ADMIN_FIRST && action->id < DEMO_INTENT_ADMIN_FIRST + DEMO_ADMIN_COUNT)
    {
        unsigned row = action->id - DEMO_INTENT_ADMIN_FIRST;
        unsigned maximum = row == 0 ? 2 : 1;
        if (action->value < 0 || action->value > maximum || floorf(action->value) != action->value)
            return false;
        if (row == 0)
        {
            /* App 更新待重启配置；运行中不重配设备，NVM 仍由统一服务持久化。 */
            meter_can_rate_t rate = (meter_can_rate_t)(unsigned)action->value;
            if (snapshot->can_rate != rate)
            {
                snapshot->can_rate = rate;
                ++snapshot->revision;
            }
        }
        else
            /* 其余三项仍为易失性演示偏好，不修改小时累计或擦除介质。 */
            admin_values[row] = (unsigned)action->value;
        return true;
    }
    if (action->id >= DEMO_INTENT_REMOTE_FIRST && action->id < DEMO_INTENT_REMOTE_FIRST + DEMO_REMOTE_COUNT)
    {
        unsigned row = action->id - DEMO_INTENT_REMOTE_FIRST;
        if (pending_valid)
            return false;
        meter_parameter_admission_t result = meter_parameters_submit(
            &parameters, remote_catalog[row].key, METER_PARAMETER_WRITE, action->value,
            (meter_parameter_policy_t){500, 200, 1}, &grant, now_ms, &pending);
        pending_valid = result == METER_PARAMETER_ACCEPTED;
        parameter_feedback = pending_valid ? DEMO_FEEDBACK_QUEUED : DEMO_FEEDBACK_INVALID;
        return pending_valid;
    }
    return false;
}
void demo_settings_run(uint32_t now_ms, const meter_command_port_t *commands)
{
    (void)commands;
    app_now = now_ms;
    if (!initialize())
        return;
    if (grant.permissions && !meter_authorization_allows(&grant, USER_PERMISSION, now_ms))
    {
        meter_authorization_revoke(&grant);
        auth_feedback = DEMO_FEEDBACK_EXPIRED;
    }
    meter_parameters_tick(&parameters, &grant, now_ms);
    if (!pending_valid)
        return;
    meter_parameter_work_t work;
    if (meter_parameters_take(&parameters, &grant, now_ms, &work))
    {
        /* 合成端点在同一 App 步内完成，不存在未排空的真实链路或私有 CAN 编码。 */
        meter_parameter_reply_t reply = {.request = work, .code = METER_PARAMETER_REPLY_OK,
                                          .has_value = true, .value = work.value};
        if (!meter_parameters_reply(&parameters, &reply, &grant, now_ms))
            parameter_feedback = DEMO_FEEDBACK_INVALID;
    }
    meter_parameter_result_t result;
    if (meter_parameters_query(&parameters, pending, &result))
    {
        bool succeeded = result.outcome == METER_PARAMETER_SUCCEEDED && result.has_value;
        parameter_feedback = succeeded ? DEMO_FEEDBACK_APPLIED : DEMO_FEEDBACK_DENIED;
        if (succeeded)
            for (unsigned i = 0; i < DEMO_REMOTE_COUNT; ++i)
                if (meter_parameter_key_equal(remote_catalog[i].key, result.request.key))
                    remote_values[i] = result.value;
        if (meter_parameters_acknowledge(&parameters, pending))
            pending_valid = false;
    }
}
static void publish(meter_snapshot_t *snapshot, meter_signal_id_t id, float value)
{
    size_t index = meter_catalog_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->signal_count)
        return;
    meter_value_t *slot = &snapshot->signals[index];
    if (slot->state != METER_VALUE_VALID || slot->value != value)
        ++snapshot->revision;
    *slot = (meter_value_t){value, app_now, METER_VALUE_VALID, METER_SOURCE_NONE};
}
void demo_settings_publish(meter_snapshot_t *snapshot)
{
    bool user = meter_authorization_allows(&grant, USER_PERMISSION, app_now);
    bool admin = meter_authorization_allows(&grant, ADMIN_PERMISSION, app_now);
    publish(snapshot, DEMO_ACCESS_ROLE, admin ? 2 : user ? 1 : 0);
    publish(snapshot, DEMO_ACCESS_RESULT, (float)auth_feedback);
    publish(snapshot, DEMO_REMOTE_RESULT, (float)parameter_feedback);
    const meter_signal_id_t admin_ids[] = {DEMO_ADMIN_CAN, DEMO_ADMIN_HOURS, DEMO_ADMIN_SPEED, DEMO_ADMIN_MEMORY};
    const meter_signal_id_t remote_ids[] = {DEMO_REMOTE_SPEED, DEMO_REMOTE_RAMP, DEMO_REMOTE_LIFT, DEMO_REMOTE_REGEN};
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
        publish(snapshot, admin_ids[i], i == 0 ? (float)snapshot->can_rate : (float)admin_values[i]);
    for (unsigned i = 0; i < DEMO_REMOTE_COUNT; ++i)
        publish(snapshot, remote_ids[i], remote_values[i]);
}
