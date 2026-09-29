#include "contracts/meter_time.h"
#include "runtime/meter_parameters.h"
#include <assert.h>
#include <math.h>

/* Synthetic engineering values; no vehicle protocol or credentials. */
static const meter_parameter_definition_t catalog[] = {
    {{1u, 7u}, true, true, true, true, -10.0f, 100.0f, 0u, 1u},
    {{2u, 7u}, true, true, true, false, 0.0f, 20.0f, 2u, 2u},
    {.key = {3u, 7u}}};
static const meter_parameter_policy_t once = {100u, 20u, 1u};
static const meter_parameter_policy_t retry = {100u, 20u, 2u};

static meter_request_id_t submit_read(meter_parameters_t *service, uint32_t now)
{
    meter_request_id_t id;
    assert(meter_parameters_submit(service, catalog[0].key, METER_PARAMETER_READ, 0.0f, retry, NULL, now,
                                   &id) == METER_PARAMETER_ACCEPTED);
    return id;
}

static meter_parameter_work_t take(meter_parameters_t *service, const meter_authorization_t *auth,
                                   uint32_t now)
{
    meter_parameter_work_t work;
    assert(meter_parameters_take(service, auth, now, &work));
    return work;
}

static meter_parameter_result_t result(meter_parameters_t *service, meter_request_id_t id,
                                       meter_parameter_outcome_t expected)
{
    meter_parameter_result_t output;
    assert(meter_parameters_query(service, id, &output));
    assert(output.outcome == expected);
    return output;
}

static void test_catalog_and_admission(void)
{
    meter_parameters_t service = {0};
    meter_parameter_definition_t invalid[2] = {catalog[0], catalog[0]};
    assert(!meter_parameters_init(&service, invalid, 2u, 1u));
    invalid[1] = catalog[1];
    assert(meter_parameters_init(&service, invalid, 2u, 1u)); /* Same ID, different owner is legal. */
    invalid[0].minimum = NAN;
    assert(!meter_parameters_init(&service, invalid, 2u, 1u));
    invalid[0] = catalog[0];
    invalid[0].write_permissions = 0u;
    assert(!meter_parameters_init(&service, invalid, 2u, 1u));
    assert(!meter_parameters_init(&service, catalog, 3u, 0u));
    assert(meter_parameters_init(&service, catalog, 3u, 9u));
    meter_request_id_t id = {90u, 90u};
    const meter_parameter_key_t missing = {9u, 7u};
    assert(meter_parameters_submit(&service, missing, METER_PARAMETER_READ, 0.0f, once, NULL, 0u, &id) ==
           METER_PARAMETER_NOT_FOUND);
    assert(meter_parameters_submit(&service, catalog[2].key, METER_PARAMETER_WRITE, 0.0f, once, NULL, 0u,
                                   &id) == METER_PARAMETER_UNCONFIRMED);
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 1.0f, once, NULL, 0u,
                                   &id) == METER_PARAMETER_DENIED);
    meter_authorization_t auth = {0};
    assert(meter_authorization_grant(&auth, 3u, 0u, 100u));
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, NAN, once, &auth, 0u,
                                   &id) == METER_PARAMETER_OUT_OF_RANGE);
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 101.0f, once, &auth, 0u,
                                   &id) == METER_PARAMETER_OUT_OF_RANGE);
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 1.0f, retry, &auth, 0u,
                                   &id) == METER_PARAMETER_INVALID);
    assert(meter_parameters_submit(&service, catalog[1].key, METER_PARAMETER_READ, 0.0f, retry, &auth, 0u,
                                   &id) == METER_PARAMETER_INVALID);
    meter_parameter_policy_t invalid_policy = {METER_TIME_HALF_RANGE, 20u, 1u};
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_READ, 0.0f, invalid_policy, NULL,
                                   0u, &id) == METER_PARAMETER_INVALID);
    assert(id.session == 90u && id.serial == 90u);
    assert(service.ledger.next_serial == 1u);
    assert(meter_parameters_submit(&service, catalog[1].key, METER_PARAMETER_READ, 0.0f, once, &auth, 0u,
                                   &id) == METER_PARAMETER_ACCEPTED);
    assert(take(&service, &auth, 0u).key.owner == 2u);
}

static void test_correlation_and_retained_result(void)
{
    meter_parameters_t service;
    assert(meter_parameters_init(&service, catalog, 3u, 9u));
    meter_request_id_t id = submit_read(&service, 0u);
    meter_parameter_work_t work = take(&service, NULL, 0u);
    meter_parameter_reply_t reply = {work, METER_PARAMETER_REPLY_OK, true, 12.0f, 17};
    meter_parameter_reply_t wrong = reply;
    wrong.request.key.owner = 2u;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    wrong = reply;
    wrong.request.key.id++;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    wrong = reply;
    wrong.request.id.session++;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    wrong = reply;
    wrong.request.id.serial++;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    wrong = reply;
    wrong.request.operation = METER_PARAMETER_WRITE;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    wrong = reply;
    wrong.request.attempt++;
    assert(!meter_parameters_reply(&service, &wrong, NULL, 1u));
    assert(!meter_parameters_acknowledge(&service, id));
    assert(meter_parameters_reply(&service, &reply, NULL, 2u));
    meter_parameter_result_t output = result(&service, id, METER_PARAMETER_SUCCEEDED);
    assert(output.has_value && output.value == 12.0f && !output.effect_unknown && output.detail == 17);
    assert(!meter_parameters_reply(&service, &reply, NULL, 3u));
    assert(!meter_parameters_cancel(&service, id));
    meter_request_id_t other;
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_READ, 0.0f, once, NULL, 3u,
                                   &other) == METER_PARAMETER_BUSY);
    assert(result(&service, id, METER_PARAMETER_SUCCEEDED).value == 12.0f);
    assert(meter_parameters_acknowledge(&service, id));
    assert(!meter_parameters_query(&service, id, &output));
    other = submit_read(&service, 4u);
    assert(other.serial != id.serial);
    (void)take(&service, NULL, 4u);
    assert(!meter_parameters_reply(&service, &reply, NULL, 5u));
}

static void test_timeouts_retry_and_wrap(void)
{
    meter_parameters_t service;
    assert(meter_parameters_init(&service, catalog, 3u, 1u));
    const uint32_t start = UINT32_MAX - 10u;
    meter_request_id_t id = submit_read(&service, start);
    meter_parameter_work_t first = take(&service, NULL, start);
    meter_parameter_work_t second;
    assert(!meter_parameters_take(&service, NULL, start + 19u, &second));
    second = take(&service, NULL, start + 20u);
    assert(second.attempt == 2u && meter_request_id_equal(first.id, second.id));
    meter_parameter_reply_t late = {first, METER_PARAMETER_REPLY_OK, true, 1.0f, 0};
    assert(!meter_parameters_reply(&service, &late, NULL, start + 21u));
    meter_parameters_tick(&service, NULL, start + 40u);
    assert(!result(&service, id, METER_PARAMETER_TIMED_OUT).effect_unknown);
    assert(!meter_parameters_take(&service, NULL, start + 41u, &second));
    assert(meter_parameters_acknowledge(&service, id));
    id = submit_read(&service, 100u);
    assert(!meter_parameters_take(&service, NULL, 200u, &second));
    assert(result(&service, id, METER_PARAMETER_TIMED_OUT).request.attempt == 0u);
    assert(meter_parameters_acknowledge(&service, id));
    id = submit_read(&service, 300u);
    first = take(&service, NULL, 390u);
    late.request = first;
    assert(!meter_parameters_reply(&service, &late, NULL, 400u));
    (void)result(&service, id, METER_PARAMETER_TIMED_OUT);
}

static void test_authorization_lifetime(void)
{
    meter_authorization_t auth = {0};
    assert(!meter_authorization_allows(&auth, 1u, 0u));
    assert(meter_authorization_allows(NULL, 0u, 0u));
    assert(!meter_authorization_grant(&auth, 1u, 0u, METER_TIME_HALF_RANGE));
    assert(!meter_authorization_grant(&auth, 1u, 0u, 0u));
    assert(meter_authorization_grant(&auth, 1u, UINT32_MAX - 4u, 10u));
    assert(meter_authorization_allows(&auth, 1u, 4u));
    assert(!meter_authorization_allows(&auth, 1u, 5u));
    assert(!meter_authorization_allows(&auth, 3u, 4u));
    meter_parameters_t service;
    assert(meter_parameters_init(&service, catalog, 3u, 1u));
    assert(meter_authorization_grant(&auth, 1u, 100u, 10u));
    meter_request_id_t id;
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 5.0f, once, &auth, 100u,
                                   &id) == METER_PARAMETER_ACCEPTED);
    meter_parameter_work_t work;
    assert(!meter_parameters_take(&service, &auth, 110u, &work));
    assert(!result(&service, id, METER_PARAMETER_PERMISSION_LOST).effect_unknown);
    assert(meter_parameters_acknowledge(&service, id));
    assert(meter_authorization_grant(&auth, 1u, 120u, 50u));
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 5.0f, once, &auth, 120u,
                                   &id) == METER_PARAMETER_ACCEPTED);
    work = take(&service, &auth, 120u);
    meter_authorization_revoke(&auth);
    assert(meter_authorization_grant(&auth, 1u, 121u, 50u));
    meter_parameter_reply_t reply = {work, METER_PARAMETER_REPLY_OK, false, 0.0f, 0};
    assert(!meter_parameters_reply(&service, &reply, &auth, 121u));
    assert(result(&service, id, METER_PARAMETER_PERMISSION_LOST).effect_unknown);
    auth.epoch = UINT64_MAX;
    meter_authorization_revoke(&auth);
    assert(!meter_authorization_grant(&auth, 1u, 122u, 50u));
    assert(!meter_authorization_allows(&auth, 1u, 122u));
}

static void test_reply_failures_and_write_uncertainty(void)
{
    meter_parameters_t service;
    assert(meter_parameters_init(&service, catalog, 3u, 1u));
    for (unsigned kind = 0u; kind < 5u; ++kind)
    {
        meter_request_id_t id = submit_read(&service, 0u);
        meter_parameter_work_t work = take(&service, NULL, 0u);
        meter_parameter_reply_t reply = {work, METER_PARAMETER_REPLY_OK, true, 1.0f, 0};
        meter_parameter_outcome_t expected = METER_PARAMETER_INVALID_REPLY;
        if (kind == 0u)
            reply.has_value = false;
        if (kind == 1u)
            reply.value = NAN;
        if (kind == 2u)
            reply.value = 101.0f;
        if (kind == 3u)
        {
            reply.code = METER_PARAMETER_REPLY_REJECTED;
            expected = METER_PARAMETER_REMOTE_REJECTED;
        }
        if (kind == 4u)
        {
            reply.code = METER_PARAMETER_REPLY_TRANSPORT_FAILED;
            expected = METER_PARAMETER_TRANSPORT_FAILED;
        }
        assert(meter_parameters_reply(&service, &reply, NULL, 1u));
        assert(!result(&service, id, expected).has_value);
        assert(meter_parameters_acknowledge(&service, id));
    }
    meter_authorization_t auth = {0};
    assert(meter_authorization_grant(&auth, 1u, 0u, 500u));
    for (unsigned kind = 0u; kind < 4u; ++kind)
    {
        meter_request_id_t id;
        assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_WRITE, 5.0f, once, &auth, 0u,
                                       &id) == METER_PARAMETER_ACCEPTED);
        meter_parameter_work_t work = {0};
        if (kind != 0u)
            work = take(&service, &auth, 0u);
        meter_parameter_outcome_t expected = METER_PARAMETER_CANCELLED;
        if (kind < 2u)
            assert(meter_parameters_cancel(&service, id));
        if (kind == 2u)
        {
            meter_parameters_tick(&service, &auth, 20u);
            expected = METER_PARAMETER_TIMED_OUT;
        }
        if (kind == 3u)
        {
            meter_parameter_reply_t reply = {work, METER_PARAMETER_REPLY_TRANSPORT_FAILED, false, 0.0f, 0};
            assert(meter_parameters_reply(&service, &reply, &auth, 1u));
            expected = METER_PARAMETER_TRANSPORT_FAILED;
        }
        assert(result(&service, id, expected).effect_unknown == (kind != 0u));
        assert(meter_parameters_acknowledge(&service, id));
    }
    service.ledger.next_serial = UINT64_MAX;
    meter_request_id_t last = submit_read(&service, 0u);
    assert(last.serial == UINT64_MAX);
    assert(meter_parameters_cancel(&service, last));
    assert(meter_parameters_acknowledge(&service, last));
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_READ, 0.0f, once, NULL, 1u,
                                   &last) == METER_PARAMETER_EXHAUSTED);
}

static void test_successful_write_and_retry_recovery(void)
{
    meter_parameters_t service;
    meter_authorization_t auth = {0};
    assert(meter_parameters_init(&service, catalog, 3u, 1u));
    assert(meter_authorization_grant(&auth, 3u, 0u, 500u));
    meter_request_id_t id;
    assert(meter_parameters_submit(&service, catalog[1].key, METER_PARAMETER_WRITE, 21.0f, once, &auth, 0u,
                                   &id) == METER_PARAMETER_OUT_OF_RANGE);
    assert(meter_parameters_submit(&service, catalog[1].key, METER_PARAMETER_WRITE, 5.0f, once, &auth, 0u,
                                   &id) == METER_PARAMETER_ACCEPTED);
    meter_parameter_work_t work = take(&service, &auth, 0u);
    assert(work.value == 5.0f && work.key.owner == 2u && work.attempt == 1u);
    meter_parameter_reply_t reply = {work, METER_PARAMETER_REPLY_OK, false, NAN, 0};
    assert(meter_parameters_reply(&service, &reply, &auth, 1u));
    meter_parameter_result_t output = result(&service, id, METER_PARAMETER_SUCCEEDED);
    assert(!output.effect_unknown && !output.has_value && output.value == 0.0f);
    assert(meter_parameters_acknowledge(&service, id));

    id = submit_read(&service, 10u);
    (void)take(&service, NULL, 10u);
    work = take(&service, NULL, 30u);
    assert(work.attempt == 2u);
    reply = (meter_parameter_reply_t){work, METER_PARAMETER_REPLY_OK, true, 9.0f, 0};
    assert(meter_parameters_reply(&service, &reply, NULL, 31u));
    output = result(&service, id, METER_PARAMETER_SUCCEEDED);
    assert(output.has_value && output.value == 9.0f && output.request.attempt == 2u);
    assert(meter_parameters_acknowledge(&service, id));

    const meter_parameter_policy_t many_attempts = {1000u, 1u, UINT8_MAX};
    assert(meter_parameters_submit(&service, catalog[0].key, METER_PARAMETER_READ, 0.0f, many_attempts, NULL,
                                   0u, &id) == METER_PARAMETER_ACCEPTED);
    for (uint32_t attempt = 1u; attempt <= UINT8_MAX; ++attempt)
    {
        work = take(&service, NULL, attempt - 1u);
        assert(work.attempt == attempt);
    }
    assert(!meter_parameters_take(&service, NULL, UINT8_MAX, &work));
    assert(result(&service, id, METER_PARAMETER_TIMED_OUT).request.attempt == UINT8_MAX);
}

int main(void)
{
    test_catalog_and_admission();
    test_correlation_and_retained_result();
    test_timeouts_retry_and_wrap();
    test_authorization_lifetime();
    test_reply_failures_and_write_uncertainty();
    test_successful_write_and_retry_recovery();
    return 0;
}
