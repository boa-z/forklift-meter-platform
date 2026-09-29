#include "runtime/meter_parameters.h"
#include "contracts/meter_time.h"
#include <math.h>

static bool pending(const meter_parameters_t *service)
{
    return (service != NULL) && ((service->slot.state == METER_REQUEST_SLOT_QUEUED) ||
                                 (service->slot.state == METER_REQUEST_SLOT_ACTIVE));
}

static bool value_in_range(const meter_parameter_definition_t *definition, float value)
{
    return isfinite(value) && (value >= definition->minimum) && (value <= definition->maximum);
}

static const meter_parameter_definition_t *find_definition(const meter_parameters_t *service,
                                                           meter_parameter_key_t key)
{
    for (size_t index = 0u; index < service->count; ++index)
    {
        if (meter_parameter_key_equal(service->catalog[index].key, key))
        {
            return &service->catalog[index];
        }
    }
    return NULL;
}

/* Every exit reserves its result until acknowledgement. A logical cancellation cannot undo I/O. */
static void complete(meter_parameters_t *service, meter_parameter_outcome_t outcome, bool effect_unknown,
                     bool has_value, float value, int32_t detail)
{
    meter_result_code_t code = METER_RESULT_FAILED;
    if (effect_unknown)
    {
        code = METER_RESULT_OUTCOME_UNKNOWN;
    }
    else if (outcome == METER_PARAMETER_SUCCEEDED)
    {
        code = METER_RESULT_APPLIED;
    }
    bool recorded;
    if (service->slot.state == METER_REQUEST_SLOT_QUEUED)
    {
        recorded = meter_requests_cancel(&service->ledger, service->work.id);
    }
    else
    {
        const meter_request_result_t ledger_result = {service->work.id, code, 0u, detail};
        recorded = meter_requests_finish(&service->ledger, &ledger_result);
    }
    if (recorded)
    {
        service->result =
            (meter_parameter_result_t){service->work, outcome, effect_unknown, has_value, value, detail};
        service->waiting = false;
    }
}

static bool dispatched_write(const meter_parameters_t *service)
{
    return (service->work.operation == METER_PARAMETER_WRITE) && (service->work.attempt != 0u);
}

bool meter_parameters_init(meter_parameters_t *service, const meter_parameter_definition_t *catalog,
                           size_t count, uint32_t session)
{
    if ((service == NULL) || (catalog == NULL) || (count == 0u) || (session == 0u) ||
        (count > SIZE_MAX / sizeof(*catalog)))
    {
        return false;
    }
    for (size_t index = 0u; index < count; ++index)
    {
        const meter_parameter_definition_t *definition = &catalog[index];
        if (definition->key.owner == 0u)
        {
            return false;
        }
        if (definition->confirmed && (!isfinite(definition->minimum) || !isfinite(definition->maximum) ||
                                      (definition->minimum > definition->maximum) ||
                                      (!definition->readable && !definition->writable) ||
                                      (definition->writable && (definition->write_permissions == 0u)) ||
                                      (definition->repeatable_read && !definition->readable)))
        {
            return false;
        }
        for (size_t previous = 0u; previous < index; ++previous)
        {
            if (meter_parameter_key_equal(catalog[previous].key, definition->key))
            {
                return false;
            }
        }
    }
    *service = (meter_parameters_t){0};
    service->catalog = catalog;
    service->count = count;
    return meter_requests_init(&service->ledger, &service->slot, 1u, session);
}

meter_parameter_admission_t meter_parameters_submit(meter_parameters_t *service, meter_parameter_key_t key,
                                                    meter_parameter_operation_t operation, float value,
                                                    meter_parameter_policy_t policy,
                                                    const meter_authorization_t *authorization,
                                                    uint32_t now_ms, meter_request_id_t *id)
{
    if ((service == NULL) || (service->catalog == NULL) || (id == NULL) ||
        ((operation != METER_PARAMETER_READ) && (operation != METER_PARAMETER_WRITE)) ||
        (policy.attempt_timeout_ms == 0u) || (policy.total_timeout_ms >= METER_TIME_HALF_RANGE) ||
        (policy.total_timeout_ms < policy.attempt_timeout_ms) || (policy.max_attempts == 0u))
    {
        return METER_PARAMETER_INVALID;
    }
    if (service->slot.state != METER_REQUEST_SLOT_FREE)
    {
        return METER_PARAMETER_BUSY;
    }
    const meter_parameter_definition_t *definition = find_definition(service, key);
    if (definition == NULL)
    {
        return METER_PARAMETER_NOT_FOUND;
    }
    if (!definition->confirmed)
    {
        return METER_PARAMETER_UNCONFIRMED;
    }
    const bool writing = operation == METER_PARAMETER_WRITE;
    const uint32_t permissions = writing ? definition->write_permissions : definition->read_permissions;
    if ((writing && !definition->writable) || (!writing && !definition->readable) ||
        !meter_authorization_allows(authorization, permissions, now_ms))
    {
        return METER_PARAMETER_DENIED;
    }
    if ((policy.max_attempts > 1u) && (writing || !definition->repeatable_read))
    {
        return METER_PARAMETER_INVALID;
    }
    if (writing && !value_in_range(definition, value))
    {
        return METER_PARAMETER_OUT_OF_RANGE;
    }
    const meter_request_admission_t admission = meter_requests_reserve(&service->ledger, id);
    switch (admission)
    {
    case METER_REQUEST_BUSY:
        return METER_PARAMETER_BUSY;
    case METER_REQUEST_INVALID:
        return METER_PARAMETER_INVALID;
    case METER_REQUEST_EXHAUSTED:
        return METER_PARAMETER_EXHAUSTED;
    case METER_REQUEST_QUEUED:
        break;
    default:
        return METER_PARAMETER_INVALID;
    }
    service->definition = definition;
    service->work = (meter_parameter_work_t){*id, key, operation, writing ? value : 0.0f, 0u};
    service->policy = policy;
    service->total_deadline_ms = now_ms + policy.total_timeout_ms;
    service->permissions = permissions;
    service->authorization_epoch = permissions == 0u ? 0u : authorization->epoch;
    service->waiting = false;
    return METER_PARAMETER_ACCEPTED;
}

void meter_parameters_tick(meter_parameters_t *service, const meter_authorization_t *authorization,
                           uint32_t now_ms)
{
    if (!pending(service))
    {
        return;
    }
    if (!meter_authorization_allows(authorization, service->permissions, now_ms) ||
        ((service->permissions != 0u) && (authorization->epoch != service->authorization_epoch)))
    {
        complete(service, METER_PARAMETER_PERMISSION_LOST, dispatched_write(service), false, 0.0f, 0);
        return;
    }
    if (meter_time_reached(now_ms, service->total_deadline_ms))
    {
        complete(service, METER_PARAMETER_TIMED_OUT, dispatched_write(service), false, 0.0f, 0);
        return;
    }
    if (service->waiting && meter_time_reached(now_ms, service->attempt_deadline_ms))
    {
        if (service->work.attempt < service->policy.max_attempts)
        {
            service->waiting = false; /* Only confirmed repeatable reads can reach another attempt. */
        }
        else
        {
            complete(service, METER_PARAMETER_TIMED_OUT, dispatched_write(service), false, 0.0f, 0);
        }
    }
}

bool meter_parameters_take(meter_parameters_t *service, const meter_authorization_t *authorization,
                           uint32_t now_ms, meter_parameter_work_t *out)
{
    if (out == NULL)
    {
        return false;
    }
    meter_parameters_tick(service, authorization, now_ms);
    if (!pending(service) || service->waiting)
    {
        return false;
    }
    if ((service->slot.state == METER_REQUEST_SLOT_QUEUED) &&
        !meter_requests_start(&service->ledger, service->work.id))
    {
        return false;
    }
    service->work.attempt++;
    service->attempt_deadline_ms = now_ms + service->policy.attempt_timeout_ms;
    service->waiting = true;
    *out = service->work;
    return true;
}

bool meter_parameters_reply(meter_parameters_t *service, const meter_parameter_reply_t *reply,
                            const meter_authorization_t *authorization, uint32_t now_ms)
{
    if (reply == NULL)
    {
        return false;
    }
    meter_parameters_tick(service, authorization, now_ms);
    if (!pending(service) || !service->waiting ||
        !meter_request_id_equal(service->work.id, reply->request.id) ||
        !meter_parameter_key_equal(service->work.key, reply->request.key) ||
        (service->work.operation != reply->request.operation) ||
        (service->work.attempt != reply->request.attempt))
    {
        return false;
    }
    switch (reply->code)
    {
    case METER_PARAMETER_REPLY_OK:
        if (((service->work.operation == METER_PARAMETER_READ) && !reply->has_value) ||
            (reply->has_value && !value_in_range(service->definition, reply->value)))
        {
            complete(service, METER_PARAMETER_INVALID_REPLY, dispatched_write(service), false, 0.0f,
                     reply->detail);
        }
        else
        {
            complete(service, METER_PARAMETER_SUCCEEDED, false, reply->has_value,
                     reply->has_value ? reply->value : 0.0f, reply->detail);
        }
        break;
    case METER_PARAMETER_REPLY_REJECTED:
        complete(service, METER_PARAMETER_REMOTE_REJECTED, false, false, 0.0f, reply->detail);
        break;
    case METER_PARAMETER_REPLY_TRANSPORT_FAILED:
        complete(service, METER_PARAMETER_TRANSPORT_FAILED, dispatched_write(service), false, 0.0f,
                 reply->detail);
        break;
    default:
        complete(service, METER_PARAMETER_INVALID_REPLY, dispatched_write(service), false, 0.0f,
                 reply->detail);
        break;
    }
    return true;
}

bool meter_parameters_cancel(meter_parameters_t *service, meter_request_id_t id)
{
    if (!pending(service) || !meter_request_id_equal(service->work.id, id))
    {
        return false;
    }
    complete(service, METER_PARAMETER_CANCELLED, dispatched_write(service), false, 0.0f, 0);
    return true;
}

bool meter_parameters_query(const meter_parameters_t *service, meter_request_id_t id,
                            meter_parameter_result_t *out)
{
    if ((service == NULL) || (out == NULL) || (service->slot.state != METER_REQUEST_SLOT_TERMINAL) ||
        !meter_request_id_equal(service->work.id, id))
    {
        return false;
    }
    *out = service->result;
    return true;
}

bool meter_parameters_acknowledge(meter_parameters_t *service, meter_request_id_t id)
{
    return (service != NULL) && meter_requests_acknowledge(&service->ledger, id);
}
