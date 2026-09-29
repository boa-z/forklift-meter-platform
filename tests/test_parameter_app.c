#include "examples/parameter-workflow/app.h"
#include <assert.h>
#include <math.h>

/* 合成协议所有者采用复制的单槽交接。App 取消或确认领取后仍保持忙碌，
 * 直到显式完成或排空。 */
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
static reference_parameter_intent_t intent(reference_parameter_field_t field,
                                           meter_parameter_operation_t operation)
{
    return (reference_parameter_intent_t){
        .profile_generation = 5u, .view_token = 41u, .field = field, .operation = operation, .value = 8.0f};
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
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_WRITE);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_DENIED);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 200u));
    input.profile_generation = 4u;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_INVALID);
    input.profile_generation = 5u;
    input.field = REFERENCE_FIELD_AUXILIARY_LIMIT;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_UNCONFIRMED);
    input.field = (reference_parameter_field_t)99;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_NOT_FOUND);
    input.field = REFERENCE_FIELD_LIFT_LIMIT;
    input.value = 11.0f;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_OUT_OF_RANGE);
    input.value = 8.0f;
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    input.value = 99.0f;
    input.field = REFERENCE_FIELD_TRAVEL_LIMIT; /* 调用方可释放或复用意图对象。 */
    reference_parameter_app_step(&app, &channel, 1u);
    assert(backend.work.value == 8.0f && backend.work.key.owner == 2u && backend.work.key.id == 7u);
    reference_parameter_view_t pending = present(&app);
    assert(pending.pending && !pending.has_result);
    assert(reference_parameter_view_matches(&pending, 5u, 41u));
    assert(!reference_parameter_view_matches(&pending, 5u, 42u));
    assert(!reference_parameter_view_matches(&pending, 6u, 41u));
    assert(!reference_parameter_app_acknowledge(&app, 41u, pending.request));
    complete(&backend, 8.0f);
    reference_parameter_app_step(&app, &channel, 2u);
    reference_parameter_view_t terminal = present(&app);
    assert(terminal.has_result && !terminal.pending && terminal.value == 8.0f);
    assert(terminal.outcome == METER_PARAMETER_SUCCEEDED && !terminal.effect_unknown);
    assert(pending.pending && !pending.has_result); /* 发布结果是独立值副本。 */
    assert(reference_parameter_app_submit(&app, &input, 3u) == METER_PARAMETER_BUSY);
    assert(!reference_parameter_app_acknowledge(&app, 42u, terminal.request));
    acknowledge(&app);
    assert(!present(&app).has_result && terminal.value == 8.0f);
}

static void cancelled_write_keeps_backend_ownership(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 200u));
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_WRITE);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 0u);
    reference_parameter_view_t old = present(&app);
    assert(!reference_parameter_app_cancel(&app, 42u, old.request));
    assert(reference_parameter_app_cancel(&app, 41u, old.request));
    reference_parameter_app_step(&app, &channel, 1u);
    assert(present(&app).outcome == METER_PARAMETER_CANCELLED);
    assert(present(&app).effect_unknown && backend.busy);
    acknowledge(&app);
    input.view_token = 42u;
    input.operation = METER_PARAMETER_READ;
    assert(reference_parameter_app_submit(&app, &input, 2u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 2u);
    assert(backend.sends == 1u && present(&app).pending);
    complete(&backend, 8.0f); /* 旧写入在调用者取消后完成。 */
    reference_parameter_app_step(&app, &channel, 3u);
    assert(backend.sends == 2u && present(&app).pending && !present(&app).has_result);
    assert(!reference_parameter_app_acknowledge(&app, 41u, old.request));
    complete(&backend, 9.0f);
    reference_parameter_app_step(&app, &channel, 4u);
    assert(present(&app).value == 9.0f);
    reference_parameter_view_t current = present(&app);
    assert(reference_parameter_view_matches(&current, 5u, 42u));
}

static void authorization_and_queue_failures(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {.blocked = true};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_WRITE);
    assert(meter_authorization_grant(&app.authorization, 1u, 0u, 10u));
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 10u);
    assert(present(&app).outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(!present(&app).effect_unknown && backend.sends == 0u);
    acknowledge(&app);
    backend.blocked = false;
    assert(meter_authorization_grant(&app.authorization, 1u, 10u, 200u));
    assert(reference_parameter_app_submit(&app, &input, 10u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 10u);
    meter_authorization_revoke(&app.authorization);
    assert(meter_authorization_grant(&app.authorization, 1u, 11u, 200u));
    complete(&backend, 8.0f);
    reference_parameter_app_step(&app, &channel, 11u);
    assert(present(&app).outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(present(&app).effect_unknown);
    acknowledge(&app);
    backend.reject_send = true;
    assert(reference_parameter_app_submit(&app, &input, 12u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 12u);
    assert(present(&app).outcome == METER_PARAMETER_TRANSPORT_FAILED);
    assert(present(&app).effect_unknown);
    reference_parameter_app_step(&app, &channel, 40u);
    assert(backend.sends == 2u); /* 交接失败后不重试写入。 */
}

static void retry_waits_for_drain_and_total_deadline(void)
{
    reference_parameter_app_t app;
    initialize(&app, 2u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_READ);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 0u);
    reference_parameter_app_step(&app, &channel, 20u);
    assert(backend.sends == 1u && backend.busy);
    complete(&backend, 1.0f);
    reference_parameter_app_step(&app, &channel, 21u);
    assert(backend.sends == 2u && backend.work.attempt == 2u && present(&app).pending);
    complete(&backend, NAN);
    reference_parameter_app_step(&app, &channel, 22u);
    assert(present(&app).outcome == METER_PARAMETER_INVALID_REPLY);
    acknowledge(&app);
    backend.blocked = true;
    assert(reference_parameter_app_submit(&app, &input, 23u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 123u);
    assert(present(&app).outcome == METER_PARAMETER_TIMED_OUT && backend.sends == 2u);
    assert(present(&app).attempts == 0u);
}
static void profile_replacement_retains_old_result(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1);
    assert(meter_authorization_grant(&app.authorization, 1, 0, 100));
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_WRITE);
    assert(reference_parameter_app_submit(&app, &input, 0) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 0);
    meter_profile_t profile = {.generation = 6, .family = 13, .capabilities = 8, .confirmed = true};
    assert(reference_parameter_app_profile(&app, &profile));
    assert(!reference_parameter_app_profile(&app, &profile));
    reference_parameter_app_step(&app, &channel, 1);
    reference_parameter_view_t old = present(&app);
    assert(old.has_result && old.effect_unknown && old.profile_generation == 5);
    assert(!reference_parameter_view_matches(&old, 6, input.view_token));
    assert(backend.busy && backend.sends == 1);
    assert(reference_parameter_app_submit(&app, &input, 1) == METER_PARAMETER_INVALID);
    acknowledge(&app);
    input.profile_generation = 6;
    input.view_token = 42;
    assert(reference_parameter_app_submit(&app, &input, 1) == METER_PARAMETER_DENIED);
    assert(meter_authorization_grant(&app.authorization, 1, 1, 100));
    assert(reference_parameter_app_submit(&app, &input, 1) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 2);
    assert(backend.sends == 1); /* 旧后端仍拥有该写入。 */
    complete(&backend, 8);
    reference_parameter_app_step(&app, &channel, 3);
    assert(present(&app).pending && backend.sends == 2);
    assert(!reference_parameter_app_acknowledge(&app, 41, old.request));
    profile = (meter_profile_t){.generation = 7};
    assert(reference_parameter_app_profile(&app, &profile));
    reference_parameter_app_step(&app, &channel, 4);
    assert(present(&app).effect_unknown);
    acknowledge(&app);
    input.profile_generation = 7;
    assert(reference_parameter_app_submit(&app, &input, 4) == METER_PARAMETER_INVALID);
}

static void result_availability_is_not_zero_success(void)
{
    reference_parameter_app_t app;
    initialize(&app, 1u);
    backend_t backend = {0};
    reference_parameter_port_t channel = port(&backend);
    reference_parameter_intent_t input = intent(REFERENCE_FIELD_TRAVEL_LIMIT, METER_PARAMETER_READ);
    assert(reference_parameter_app_submit(&app, &input, 0u) == METER_PARAMETER_ACCEPTED);
    assert(!present(&app).has_value && !present(&app).has_result);
    reference_parameter_app_step(&app, &channel, 1u);
    assert(backend.work.key.owner == 1u && backend.work.key.id == 7u);
    complete(&backend, 0.0f);
    reference_parameter_app_step(&app, &channel, 2u);
    reference_parameter_view_t valid_zero = present(&app);
    assert(valid_zero.has_value && valid_zero.value == 0.0f && valid_zero.attempts == 1u);
    assert(valid_zero.outcome == METER_PARAMETER_SUCCEEDED);
    acknowledge(&app);
    input.view_token++;
    assert(reference_parameter_app_submit(&app, &input, 3u) == METER_PARAMETER_ACCEPTED);
    reference_parameter_app_step(&app, &channel, 4u);
    complete(&backend, 42.0f);
    backend.reply.code = METER_PARAMETER_REPLY_REJECTED;
    reference_parameter_app_step(&app, &channel, 5u);
    reference_parameter_view_t rejected = present(&app);
    assert(rejected.has_result && !rejected.has_value && !rejected.effect_unknown);
    assert(rejected.outcome == METER_PARAMETER_REMOTE_REJECTED);
    assert(valid_zero.has_value && valid_zero.value == 0.0f); /* 此前复制的展示值保持不变。 */
}

int main(void)
{
    result_availability_is_not_zero_success();
    profile_replacement_retains_old_result();
    admission_and_copied_presentation();
    cancelled_write_keeps_backend_ownership();
    authorization_and_queue_failures();
    retry_waits_for_drain_and_total_deadline();
    return 0;
}
