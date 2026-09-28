#include "examples/parameter-workflow/app.h"
#include "contracts/meter_time.h"

/* Reference Application policy, not a production Product binding. Caller-selected
 * policy is checked by the transaction core; the UI cannot choose retry/deadline rules. */
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
    return true;
}

meter_parameter_admission_t reference_parameter_app_submit(reference_parameter_app_t *app,
                                                           const reference_parameter_intent_t *intent,
                                                           uint32_t now_ms)
{
    if (!app || !intent || intent->view_token == 0u || intent->profile_generation != app->generation)
        return METER_PARAMETER_INVALID;
    meter_request_id_t id;
    const meter_parameter_admission_t admission =
        meter_parameters_submit(&app->parameters, intent->key, intent->operation, intent->value, app->policy,
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
    /* Bounded to one reply and one dispatch per App visit. Stale replies are
     * consumed even while no caller is active, without attributing them to new work. */
    meter_parameter_reply_t reply;
    if (port->receive(port->context, &reply))
        (void)meter_parameters_reply(&app->parameters, &reply, &app->authorization, now_ms);
    meter_parameter_work_t work;
    if (port->ready(port->context) &&
        meter_parameters_take(&app->parameters, &app->authorization, now_ms, &work))
    {
        if (!port->send(port->context, &work))
        {
            /* take is the core's dispatch boundary. A failed handoff is retained
             * conservatively as transport failure, never silently retried as a write. */
            reply =
                (meter_parameter_reply_t){.request = work, .code = METER_PARAMETER_REPLY_TRANSPORT_FAILED};
            (void)meter_parameters_reply(&app->parameters, &reply, &app->authorization, now_ms);
        }
    }
    if (app->active && meter_parameters_query(&app->parameters, app->view.request, &app->view.result))
    {
        app->view.pending = false;
        app->view.has_result = true;
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
