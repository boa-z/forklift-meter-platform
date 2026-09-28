#include "runtime/meter_calibration.h"
#include <assert.h>
#include <math.h>
static const meter_signal_def_t signals[] = {{"measurement", "V", 50, 41}};
static const meter_catalog_t domain = {.signals = signals, .signal_count = 1};
static const meter_parameter_definition_t parameters[] = {{.key = {2, 9},
                                                           .confirmed = true,
                                                           .readable = true,
                                                           .writable = true,
                                                           .minimum = 0,
                                                           .maximum = 100,
                                                           .write_permissions = 1,
                                                           .read_permissions = 1}};
static const meter_calibration_definition_t mapping = {.source = 41,
                                                       .target = {2, 9},
                                                       .maximum_age_ms = 50,
                                                       .verify = true,
                                                       .tolerance = 0.1f,
                                                       .write_policy = {100, 20, 1},
                                                       .read_policy = {100, 20, 1}};
typedef struct
{
    meter_calibration_t service;
    meter_authorization_t auth;
    meter_value_t sample;
    meter_snapshot_t snapshot;
} fixture_t;
static void setup(fixture_t *f)
{
    *f = (fixture_t){0};
    assert(meter_calibration_init(&f->service, parameters, 1, 19));
    assert(meter_authorization_grant(&f->auth, 1, 0, 200));
    f->sample = (meter_value_t){12.0f, 0, METER_VALUE_VALID, 1};
    f->snapshot = (meter_snapshot_t){.catalog = &domain,
                                     .signals = &f->sample,
                                     .profile = {.generation = 7, .family = 3, .confirmed = true}};
}
static void begin(fixture_t *f)
{
    assert(meter_calibration_begin(&f->service, &mapping, &f->snapshot, 7, 99, true, &f->auth, 1));
}
static meter_parameter_work_t take(fixture_t *f, uint32_t now)
{
    meter_parameter_work_t work;
    assert(meter_calibration_take(&f->service, &f->snapshot.profile, true, &f->auth, now, &work));
    return work;
}
static void reply(fixture_t *f, meter_parameter_work_t work, meter_parameter_reply_code_t code, float value,
                  uint32_t now)
{
    meter_parameter_reply_t result = {.request = work, .code = code, .has_value = true, .value = value};
    assert(meter_calibration_reply(&f->service, &f->snapshot.profile, true, &f->auth, now, &result));
}
static void denied_source(void)
{
    for (int state = 0; state <= 3; ++state)
    {
        if (state == METER_VALUE_VALID)
            continue;
        fixture_t f;
        setup(&f);
        f.sample.state = (meter_value_state_t)state;
        begin(&f);
        assert(f.service.view.phase == METER_CALIBRATION_DONE);
        assert(f.service.view.outcome == METER_CALIBRATION_SOURCE_UNAVAILABLE);
        assert(!f.service.view.has_write_result);
    }
    fixture_t f;
    setup(&f);
    f.sample.value = NAN;
    begin(&f);
    assert(f.service.view.outcome == METER_CALIBRATION_SOURCE_UNAVAILABLE);
    setup(&f);
    f.sample.timestamp_ms = 2;
    begin(&f); /* Future timestamp is not fresh. */
    assert(f.service.view.outcome == METER_CALIBRATION_SOURCE_UNAVAILABLE);
    setup(&f);
    assert(meter_calibration_begin(&f.service, &mapping, &f.snapshot, 6, 99, true, &f.auth, 1));
    assert(f.service.view.outcome == METER_CALIBRATION_PROFILE_CHANGED);
    setup(&f);
    assert(meter_calibration_begin(&f.service, &mapping, &f.snapshot, 7, 99, false, &f.auth, 1));
    assert(f.service.view.outcome == METER_CALIBRATION_PREREQUISITE);
    setup(&f);
    begin(&f);
    meter_parameter_work_t work;
    assert(!meter_calibration_take(&f.service, &f.snapshot.profile, true, &f.auth, 50, &work));
    assert(f.service.view.outcome == METER_CALIBRATION_SOURCE_UNAVAILABLE && !f.service.view.effect_unknown);
}
static void successful_readback(void)
{
    fixture_t f;
    setup(&f);
    begin(&f);
    f.sample.value = 90; /* Captured value remains the user's selected measurement. */
    meter_parameter_work_t write = take(&f, 2);
    assert(write.value == 12);
    reply(&f, write, METER_PARAMETER_REPLY_OK, 12, 3);
    assert(f.service.view.phase == METER_CALIBRATION_READING && f.service.view.has_write_result);
    meter_parameter_work_t read = take(&f, 4);
    assert(read.operation == METER_PARAMETER_READ);
    meter_parameter_reply_t late = {.request = write, .code = METER_PARAMETER_REPLY_OK};
    assert(!meter_calibration_reply(&f.service, &f.snapshot.profile, true, &f.auth, 5, &late));
    reply(&f, read, METER_PARAMETER_REPLY_OK, 12.05f, 6);
    assert(f.service.view.phase == METER_CALIBRATION_DONE && f.service.view.outcome == METER_CALIBRATION_OK);
    meter_calibration_view_t copy;
    meter_calibration_present(&f.service, &copy);
    assert(copy.has_read_result && !copy.effect_unknown);
    assert(!meter_calibration_view_matches(&copy, &f.snapshot.profile, 100));
    assert(meter_calibration_view_matches(&copy, &f.snapshot.profile, 99));
    assert(!meter_calibration_acknowledge(&f.service, 8, 99));
    assert(!meter_calibration_acknowledge(&f.service, 7, 100));
    assert(!meter_calibration_begin(&f.service, &mapping, &f.snapshot, 7, 100, true, &f.auth, 7));
    assert(meter_calibration_acknowledge(&f.service, 7, 99));
    assert(copy.phase == METER_CALIBRATION_DONE && f.service.view.phase == METER_CALIBRATION_IDLE);
}
static void failures(void)
{
    for (unsigned scenario = 0; scenario < 6; ++scenario)
    {
        fixture_t f;
        setup(&f);
        begin(&f);
        meter_parameter_work_t work = take(&f, 2);
        if (scenario == 0)
            reply(&f, work, METER_PARAMETER_REPLY_REJECTED, 0, 3);
        if (scenario == 1)
            reply(&f, work, METER_PARAMETER_REPLY_TRANSPORT_FAILED, 0, 3);
        if (scenario == 2)
            meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 22);
        if (scenario == 3)
        {
            meter_authorization_revoke(&f.auth);
            meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 3);
        }
        if (scenario == 4)
        {
            ++f.snapshot.profile.generation;
            meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 3);
        }
        if (scenario == 5)
            meter_calibration_step(&f.service, &f.snapshot.profile, false, &f.auth, 3);
        assert(f.service.view.phase == METER_CALIBRATION_DONE && f.service.view.has_write_result);
        assert(f.service.view.write_result.outcome != METER_PARAMETER_SUCCEEDED);
        if (scenario != 0)
            assert(f.service.view.effect_unknown);
        meter_calibration_view_t copy;
        meter_calibration_present(&f.service, &copy);
        assert(copy.effect_unknown == f.service.view.effect_unknown);
        meter_parameter_reply_t late = {.request = work, .code = METER_PARAMETER_REPLY_OK};
        assert(!meter_calibration_reply(&f.service, &f.snapshot.profile, true, &f.auth, 24, &late));
    }
    fixture_t f;
    setup(&f);
    begin(&f);
    assert(meter_authorization_grant(&f.auth, 1, 0, 2));
    meter_parameter_work_t work;
    assert(!meter_calibration_take(&f.service, &f.snapshot.profile, true, &f.auth, 2, &work));
    assert(f.service.view.write_result.outcome == METER_PARAMETER_PERMISSION_LOST &&
           !f.service.view.effect_unknown);
    setup(&f);
    meter_authorization_revoke(&f.auth);
    begin(&f);
    assert(f.service.view.admission == METER_PARAMETER_DENIED &&
           f.service.view.outcome == METER_CALIBRATION_ADMISSION_FAILED);
    setup(&f);
    begin(&f);
    work = take(&f, 2);
    reply(&f, work, METER_PARAMETER_REPLY_OK, 12, 3);
    work = take(&f, 4);
    reply(&f, work, METER_PARAMETER_REPLY_OK, 13, 5);
    assert(f.service.view.outcome == METER_CALIBRATION_VERIFY_MISMATCH && f.service.view.has_write_result);
    setup(&f);
    begin(&f);
    work = take(&f, 2);
    reply(&f, work, METER_PARAMETER_REPLY_OK, 12, 3);
    work = take(&f, 4);
    reply(&f, work, METER_PARAMETER_REPLY_TRANSPORT_FAILED, 0, 5);
    assert(f.service.view.write_result.outcome == METER_PARAMETER_SUCCEEDED);
    assert(f.service.view.outcome == METER_CALIBRATION_TRANSACTION_FAILED && f.service.view.has_read_result);
}
static void lifetime_boundaries(void)
{
    fixture_t f;
    setup(&f);
    meter_calibration_definition_t no_readback = mapping;
    no_readback.verify = false;
    f.sample.timestamp_ms = UINT32_MAX - 4u;
    assert(meter_calibration_begin(&f.service, &no_readback, &f.snapshot, 7, 99, true, &f.auth, 1));
    meter_parameter_work_t work = take(&f, 2);
    reply(&f, work, METER_PARAMETER_REPLY_OK, 12, 3);
    assert(f.service.view.phase == METER_CALIBRATION_DONE);
    assert(f.service.view.outcome == METER_CALIBRATION_OK && !f.service.view.has_read_result);
    meter_calibration_view_t retained = f.service.view;
    ++f.snapshot.profile.generation;
    meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 4);
    assert(f.service.view.profile_generation == retained.profile_generation);
    assert(f.service.view.outcome == retained.outcome);
    assert(!meter_calibration_view_matches(&retained, &f.snapshot.profile, 99));

    setup(&f);
    assert(meter_authorization_grant(&f.auth, 1, 0, 5));
    begin(&f);
    work = take(&f, 2);
    meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 5);
    assert(f.service.view.write_result.outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(f.service.view.effect_unknown); /* Same grant expires; no revoke/regrant. */

    setup(&f);
    begin(&f);
    work = take(&f, 2);
    reply(&f, work, METER_PARAMETER_REPLY_OK, 12, 3);
    work = take(&f, 4);
    f.snapshot.profile.confirmed = false;
    meter_calibration_step(&f.service, &f.snapshot.profile, true, &f.auth, 5);
    assert(f.service.view.outcome == METER_CALIBRATION_PROFILE_CHANGED);
    assert(f.service.view.write_result.outcome == METER_PARAMETER_SUCCEEDED);
    assert(f.service.view.has_read_result && !f.service.view.effect_unknown);
    meter_parameter_reply_t late = {
        .request = work, .code = METER_PARAMETER_REPLY_OK, .has_value = true, .value = 12};
    assert(!meter_calibration_reply(&f.service, &f.snapshot.profile, true, &f.auth, 6, &late));

    setup(&f);
    f.snapshot.profile.confirmed = false;
    begin(&f);
    assert(f.service.view.outcome == METER_CALIBRATION_PROFILE_CHANGED);
    assert(!f.service.view.has_write_result);
}
static void malformed_begin_preserves_idle(void)
{
    fixture_t f;
    setup(&f);
    meter_calibration_definition_t bad = mapping;
    bad.maximum_age_ms = 0x80000000u;
    assert(!meter_calibration_begin(&f.service, &bad, &f.snapshot, 7, 99, true, &f.auth, 1));
    bad = mapping;
    bad.tolerance = NAN;
    assert(!meter_calibration_begin(&f.service, &bad, &f.snapshot, 7, 99, true, &f.auth, 1));
    assert(f.service.view.phase == METER_CALIBRATION_IDLE && f.service.view.token == 0);
    begin(&f);
    assert(f.service.view.phase == METER_CALIBRATION_WRITING);
}
int main(void)
{
    denied_source();
    successful_readback();
    failures();
    lifetime_boundaries();
    malformed_begin_preserves_idle();
    return 0;
}
