#include "examples/parameter-workflow/app.h"
#include "contracts/meter_time.h"

/* 参考 App 策略，不是生产 Product 绑定。事务内核校验调用方选择的策略；
 * UI 无权决定重试与期限规则。 */
bool reference_parameter_app_init(reference_parameter_app_t *app, const meter_parameter_definition_t *catalog,
                                  size_t count, uint32_t session, uint32_t generation,
                                  meter_parameter_policy_t policy)
{
    if (!app || generation == 0u || policy.max_attempts == 0u || policy.attempt_timeout_ms == 0u ||
        policy.total_timeout_ms < policy.attempt_timeout_ms ||
        policy.total_timeout_ms >= METER_TIME_HALF_RANGE)
        return false;
    if (!meter_parameters_init(&app->parameters, catalog, count, session))
        return false;
    app->authorization = (meter_authorization_t){0};
    app->view = (reference_parameter_view_t){0};
    app->policy = policy;
    app->generation = generation;
    app->active = false;
    app->profile_ready = true; /* 初始化调用方提供已确认的参考配置代数。 */
    return true;
}

/* 这些属主和对象身份仅属于本合成 Product。两个独立属主特意复用对象 7，
 * 用于验证属主隔离。真实 Product 替换此映射及目录即可，无须改动事务引擎。 */
static bool parameter_key(reference_parameter_field_t field, meter_parameter_key_t *key)
{
    switch (field)
    {
    case REFERENCE_FIELD_TRAVEL_LIMIT:
        *key = (meter_parameter_key_t){1u, 7u};
        return true;
    case REFERENCE_FIELD_LIFT_LIMIT:
        *key = (meter_parameter_key_t){2u, 7u};
        return true;
    case REFERENCE_FIELD_AUXILIARY_LIMIT:
        *key = (meter_parameter_key_t){3u, 7u};
        return true;
    default:
        return false;
    }
}

meter_parameter_admission_t reference_parameter_app_submit(reference_parameter_app_t *app,
                                                           const reference_parameter_intent_t *intent,
                                                           uint32_t now_ms)
{
    if (!app || !app->profile_ready || !intent || intent->view_token == 0u ||
        intent->profile_generation != app->generation)
        return METER_PARAMETER_INVALID;
    meter_parameter_key_t key;
    if (!parameter_key(intent->field, &key))
        return METER_PARAMETER_NOT_FOUND;
    meter_request_id_t id;
    const meter_parameter_admission_t admission =
        meter_parameters_submit(&app->parameters, key, intent->operation, intent->value, app->policy,
                                &app->authorization, now_ms, &id);
    if (admission == METER_PARAMETER_ACCEPTED)
    {
        app->view = (reference_parameter_view_t){.profile_generation = app->generation,
                                                 .view_token = intent->view_token,
                                                 .request = id,
                                                 .pending = true};
        app->active = true;
    }
    return admission;
}

void reference_parameter_app_step(reference_parameter_app_t *app, const reference_parameter_port_t *port,
                                  uint32_t now_ms)
{
    meter_parameters_tick(&app->parameters, &app->authorization, now_ms);
    /* 每次 App 调度最多消费一个回复、派发一次请求。即使没有活动调用者，
     * 也会消费旧回复，但绝不将其归属到新请求。 */
    meter_parameter_reply_t reply;
    if (port->receive(port->context, &reply))
        (void)meter_parameters_reply(&app->parameters, &reply, &app->authorization, now_ms);
    meter_parameter_work_t work;
    if (port->ready(port->context) &&
        meter_parameters_take(&app->parameters, &app->authorization, now_ms, &work))
    {
        if (!port->send(port->context, &work))
        {
            /* take 是内核的派发边界。交接失败时保守地保留传输失败结果，
             * 不得静默重试写入。 */
            reply =
                (meter_parameter_reply_t){.request = work, .code = METER_PARAMETER_REPLY_TRANSPORT_FAILED};
            (void)meter_parameters_reply(&app->parameters, &reply, &app->authorization, now_ms);
        }
    }
    meter_parameter_result_t result;
    if (app->active && meter_parameters_query(&app->parameters, app->view.request, &result))
    {
        app->view.pending = false;
        app->view.has_result = true;
        app->view.outcome = result.outcome;
        app->view.effect_unknown = result.effect_unknown;
        app->view.attempts = result.request.attempt;
        app->view.has_value = result.has_value;
        app->view.value = result.value;
    }
}

bool reference_parameter_app_cancel(reference_parameter_app_t *app, uint64_t token, meter_request_id_t id)
{
    return app->active && app->view.view_token == token && meter_parameters_cancel(&app->parameters, id);
}

bool reference_parameter_app_acknowledge(reference_parameter_app_t *app, uint64_t token,
                                         meter_request_id_t id)
{
    if (!app->active || app->view.view_token != token || !app->view.has_result ||
        !meter_parameters_acknowledge(&app->parameters, id))
        return false;
    app->active = false;
    app->view = (reference_parameter_view_t){0};
    return true;
}

void reference_parameter_app_present(const reference_parameter_app_t *app, reference_parameter_view_t *out)
{
    *out = app->view;
}

bool reference_parameter_app_profile(reference_parameter_app_t *app, const meter_profile_t *profile)
{
    if (!app || !profile || profile->generation <= app->generation ||
        (profile->confirmed && profile->family == 0u) ||
        (!profile->confirmed && (profile->family != 0u || profile->capabilities != 0u)))
        return false;
    if (app->active)
        (void)meter_parameters_cancel(&app->parameters, app->view.request);
    meter_authorization_revoke(&app->authorization);
    app->generation = profile->generation;
    app->profile_ready = profile->confirmed;
    return true;
}
