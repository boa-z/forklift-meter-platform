#include "examples/parameter-workflow/app.h"
#include <assert.h>
#include <math.h>

/* A synthetic protocol owner with a copied one-slot handoff. Its busy state
 * survives App cancellation/acknowledgement until explicit completion/drain. */
typedef struct
{
    bool busy, reply_ready, reject_send, blocked;
    unsigned sends;
    meter_parameter_work_t work;
    meter_parameter_reply_t reply;
} backend_t;
static bool ready(void *context)
{
    backend_t *backend = context;
    return !backend->busy && !backend->reply_ready && !backend->blocked;
}
static bool send_work(void *context, const meter_parameter_work_t *work)
{
    backend_t *backend = context;
    ++backend->sends;
    if (backend->reject_send)
        return false;
    assert(ready(context));
    backend->work = *work;
    backend->busy = true;
    return true;
}
static bool receive_reply(void *context, meter_parameter_reply_t *out)
{
    backend_t *backend = context;
    if (!backend->reply_ready)
        return false;
    *out = backend->reply;
    backend->reply_ready = false;
    return true;
}
static reference_parameter_port_t port(backend_t *backend)
{
    return (reference_parameter_port_t){backend, ready, send_work, receive_reply};
}
static void complete(backend_t *backend, float value)
{
    assert(backend->busy);
    backend->reply = (meter_parameter_reply_t){
        .request = backend->work, .code = METER_PARAMETER_REPLY_OK, .has_value = true, .value = value};
    backend->reply_ready = true;
    backend->busy = false;
}
static const meter_parameter_definition_t catalog[] = {{.key = {1u, 7u},
                                                        .confirmed = true,
                                                        .readable = true,
                                                        .writable = true,
                                                        .repeatable_read = true,
                                                        .minimum = 0.0f,
                                                        .maximum = 100.0f,
                                                        .write_permissions = 1u},
                                                       {.key = {2u, 7u},
                                                        .confirmed = true,
                                                        .readable = true,
                                                        .writable = true,
                                                        .minimum = 0.0f,
                                                        .maximum = 10.0f,
                                                        .read_permissions = 1u,
                                                        .write_permissions = 1u},
                                                       {.key = {3u, 7u}}};
static void initialize(reference_parameter_app_t *app, uint8_t attempts)
{
    const meter_parameter_policy_t policy = {
        .total_timeout_ms = 100u, .attempt_timeout_ms = 20u, .max_attempts = attempts};
    assert(reference_parameter_app_init(app, catalog, 3u, 11u, 5u, policy));
}
static reference_parameter_intent_t intent(uint16_t owner, meter_parameter_operation_t operation)
{
    return (reference_parameter_intent_t){.profile_generation = 5u,
                                          .view_token = 41u,
                                          .key = {owner, 7u},
                                          .operation = operation,
                                          .value = 8.0f};
}
static reference_parameter_view_t present(const reference_parameter_app_t *app)
{
    reference_parameter_view_t view;
    reference_parameter_app_present(app, &view);
    return view;
}
static void acknowledge(reference_parameter_app_t *app)
{
    reference_parameter_view_t view = present(app);
    assert(view.has_result);
    assert(reference_parameter_app_acknowledge(app, view.view_token, view.request));
}

static void admission_and_copied_presentation(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(1u, METER_PARAMETER_WRITE);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_DENIED);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 200u));
    input.profile_generation = 4u;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_INVALID);
    input.profile_generation = 5u;
    input.key.owner = 3u;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_UNCONFIRMED);
    input.key.owner = 9u;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_NOT_FOUND);
    input.key.owner = 2u;
    input.value = 11.0f;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_OUT_OF_RANGE);
    input.value = 8.0f;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    input.value = 99.0f;
    input.key.owner = 1u; /* Caller can release/reuse its intent. */
    reference_parameter_app_step(&app, &channel, 1u);
    assert(backend.work.value == 8.0f && backend.work.key.owner == 2u);
    reference_parameter_view_t pending = present(&app);
    assert(pending.pending && !pending.has_result);
    assert(reference_parameter_view_matches(&pending, 5u, 41u));
    assert(!reference_parameter_view_matches(&pending, 5u, 42u));
    assert(!reference_parameter_view_matches(&pending, 6u, 41u));
    assert(!reference_parameter_app_acknowledge(&app, 41u, pending.request));
    complete(&backend, 8.0f);
    reference_parameter_app_step(&app, &channel, 2u);
    reference_parameter_view_t terminal = present(&app);
    assert(terminal.has_result && !terminal.pending && terminal.result.value == 8.0f);
    assert(terminal.result.outcome == METER_PARAMETER_SUCCEEDED && !terminal.result.effect_unknown);
    assert(pending.pending && !pending.has_result); /* Publication is a value copy. */
    assert(reference_parameter_app_submit(&app, &input, 3u) == METER_PARAMETER_BUSY);
    assert(!reference_parameter_app_acknowledge(&app, 42u, terminal.request));
    acknowledge(&app);
    assert(!present(&app).has_result && terminal.result.value == 8.0f);
}

static void cancelled_write_keeps_backend_ownership(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 200u));
    reference_parameter_intent_t input = intent(1u, METER_PARAMETER_WRITE);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 0u);
    reference_parameter_view_t old = present(&app);
    assert(!reference_parameter_app_cancel(&app, 42u, old.request));
    assert(reference_parameter_app_cancel(&app, 41u, old.request));
    reference_parameter_app_step(&app, &channel, 1u);
    assert(present(&app).result.outcome == METER_PARAMETER_CANCELLED);
    assert(present(&app).result.effect_unknown && backend.busy);
    acknowledge(&app);
    input.view_token = 42u;
    input.operation = METER_PARAMETER_READ;
    assert(reference_parameter_app_submit(&app, &input, 2u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 2u);
    assert(backend.sends == 1u && present(&app).pending);
    complete(&backend, 8.0f); /* Old write completes after caller cancellation. */
    reference_parameter_app_step(&app, &channel, 3u);
    assert(backend.sends == 2u && present(&app).pending && !present(&app).has_result);
    assert(!reference_parameter_app_acknowledge(&app, 41u, old.request));
    complete(&backend, 9.0f);
    reference_parameter_app_step(&app, &channel, 4u);
    assert(present(&app).result.value == 9.0f);
    reference_parameter_view_t current = present(&app);
    assert(reference_parameter_view_matches(&current, 5u, 42u));
}

static void authorization_and_queue_failures(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {.blocked = true};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(1u, METER_PARAMETER_WRITE);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 10u));
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 10u);
    assert(present(&app).result.outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(!present(&app).result.effect_unknown && backend.sends == 0u);
    acknowledge(&app);
    backend.blocked = false;
    assert(meter_authorization_grant(&app.authorization, 1u, 10u, 200u));
    assert(reference_parameter_app_submit(&app, &input, 10u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 10u);
    meter_authorization_revoke(&app.authorization);
    assert(meter_authorization_grant(&app.authorization, 1u, 11u, 200u));
    complete(&backend, 8.0f);
    reference_parameter_app_step(&app, &channel, 11u);
    assert(present(&app).result.outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(present(&app).result.effect_unknown);
    acknowledge(&app);
    backend.reject_send = true;
    assert(reference_parameter_app_submit(&app, &input, 12u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 12u);
    assert(present(&app).result.outcome == METER_PARAMETER_TRANSPORT_FAILED);
    assert(present(&app).result.effect_unknown);
    reference_parameter_app_step(&app, &channel, 40u);
    assert(backend.sends == 2u); /* No write retry after handoff failure. */
}

static void retry_waits_for_drain_and_total_deadline(void)
{
    reference_parameter_app_t app;
    initialize(&app, 2u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(1u, METER_PARAMETER_READ);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 0u);
    reference_parameter_app_step(&app, &channel, 20u);
    assert(backend.sends == 1u && backend.busy);
    complete(&backend, 1.0f);
    reference_parameter_app_step(&app, &channel, 21u);
    assert(backend.sends == 2u && backend.work.attempt == 2u && present(&app).pending);
    complete(&backend, NAN);
    reference_parameter_app_step(&app, &channel, 22u);
    assert(present(&app).result.outcome == METER_PARAMETER_INVALID_REPLY);
    acknowledge(&app);
    backend.blocked = true;
    assert(reference_parameter_app_submit(&app, &input, 23u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 123u);
    assert(present(&app).result.outcome == METER_PARAMETER_TIMED_OUT && backend.sends == 2u);
    assert(present(&app).result.request.attempt == 0u);
}
int main(void)
{
    admission_and_copied_presentation();
    cancelled_write_keeps_backend_ownership();
    authorization_and_queue_failures();
    retry_waits_for_drain_and_total_deadline();
    return 0;
}
