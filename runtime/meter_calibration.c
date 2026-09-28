#include "runtime/meter_calibration.h"
#include "contracts/meter_time.h"
#include <math.h>

static bool running(const meter_calibration_t *service)
{
    return service->view.phase == METER_CALIBRATION_WRITING ||
           service->view.phase == METER_CALIBRATION_READING;
}
static void finish(meter_calibration_t *service, meter_calibration_outcome_t outcome)
{
    service->view.phase = METER_CALIBRATION_DONE;
    service->view.outcome = outcome;
}
/* Retain both stages: successful write remains observable even if readback fails. */
static void retain(meter_calibration_t *service, const meter_parameter_result_t *result)
{
    if (service->view.phase == METER_CALIBRATION_WRITING)
    {
        service->view.write_result = *result;
        service->view.has_write_result = true;
    }
    else
    {
        service->view.read_result = *result;
        service->view.has_read_result = true;
    }
    service->view.effect_unknown = service->view.effect_unknown || result->effect_unknown;
    (void)meter_parameters_acknowledge(&service->parameters, result->request.id);
}
static void invalidate(meter_calibration_t *service, meter_calibration_outcome_t outcome)
{
    (void)meter_parameters_cancel(&service->parameters, service->request);
    meter_parameter_result_t result;
    if (meter_parameters_query(&service->parameters, service->request, &result))
        retain(service, &result);
    finish(service, outcome);
}
bool meter_calibration_init(meter_calibration_t *service, const meter_parameter_definition_t *catalog,
                            size_t count, uint32_t session)
{
    if (!service || !meter_parameters_init(&service->parameters, catalog, count, session))
        return false;
    service->definition = (meter_calibration_definition_t){0};
    service->view = (meter_calibration_view_t){0};
    service->request = (meter_request_id_t){0};
    service->captured_ms = 0;
    service->dispatched = false;
    return true;
}
bool meter_calibration_begin(meter_calibration_t *service, const meter_calibration_definition_t *definition,
                             const meter_snapshot_t *snapshot, uint32_t measurement_generation,
                             uint64_t token, bool prerequisites, const meter_authorization_t *authorization,
                             uint32_t now_ms)
{
    if (!service || !definition || !snapshot || !snapshot->catalog || !authorization || token == 0u ||
        service->view.phase != METER_CALIBRATION_IDLE || definition->source == 0u ||
        definition->maximum_age_ms == 0u || definition->maximum_age_ms >= METER_TIME_HALF_RANGE ||
        !isfinite(definition->tolerance) || definition->tolerance < 0.0f)
        return false;
    service->definition = *definition;
    service->view =
        (meter_calibration_view_t){.profile_generation = snapshot->profile.generation, .token = token};
    service->dispatched = false;
    if (!meter_profile_matches(&snapshot->profile, measurement_generation))
    {
        finish(service, METER_CALIBRATION_PROFILE_CHANGED);
        return true;
    }
    meter_value_t sample = meter_snapshot_read(snapshot, definition->source);
    if (sample.state != METER_VALUE_VALID || !isfinite(sample.value) ||
        (uint32_t)(now_ms - sample.timestamp_ms) >= definition->maximum_age_ms)
    {
        finish(service, METER_CALIBRATION_SOURCE_UNAVAILABLE);
        return true;
    }
    if (!prerequisites)
    {
        finish(service, METER_CALIBRATION_PREREQUISITE);
        return true;
    }
    service->view.captured_value = sample.value;
    service->captured_ms = sample.timestamp_ms;
    service->view.admission =
        meter_parameters_submit(&service->parameters, definition->target, METER_PARAMETER_WRITE, sample.value,
                                definition->write_policy, authorization, now_ms, &service->request);
    if (service->view.admission != METER_PARAMETER_ACCEPTED)
    {
        finish(service, METER_CALIBRATION_ADMISSION_FAILED);
        return true;
    }
    service->view.phase = METER_CALIBRATION_WRITING;
    return true;
}
void meter_calibration_step(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms)
{
    if (!running(service))
        return;
    if (!meter_profile_matches(profile, service->view.profile_generation))
    {
        invalidate(service, METER_CALIBRATION_PROFILE_CHANGED);
        return;
    }
    if (!prerequisites)
    {
        invalidate(service, METER_CALIBRATION_PREREQUISITE);
        return;
    }
    if (service->view.phase == METER_CALIBRATION_WRITING && !service->dispatched &&
        (uint32_t)(now_ms - service->captured_ms) >= service->definition.maximum_age_ms)
    {
        invalidate(service, METER_CALIBRATION_SOURCE_UNAVAILABLE);
        return;
    }
    meter_parameters_tick(&service->parameters, authorization, now_ms);
    meter_parameter_result_t result;
    if (!meter_parameters_query(&service->parameters, service->request, &result))
        return;
    bool writing = service->view.phase == METER_CALIBRATION_WRITING;
    retain(service, &result);
    if (result.outcome != METER_PARAMETER_SUCCEEDED)
    {
        finish(service, METER_CALIBRATION_TRANSACTION_FAILED);
        return;
    }
    if (!writing)
    {
        bool matched = result.has_value &&
                       fabsf(result.value - service->view.captured_value) <= service->definition.tolerance;
        finish(service, matched ? METER_CALIBRATION_OK : METER_CALIBRATION_VERIFY_MISMATCH);
        return;
    }
    if (!service->definition.verify)
    {
        finish(service, METER_CALIBRATION_OK);
        return;
    }
    service->view.admission =
        meter_parameters_submit(&service->parameters, service->definition.target, METER_PARAMETER_READ, 0,
                                service->definition.read_policy, authorization, now_ms, &service->request);
    if (service->view.admission != METER_PARAMETER_ACCEPTED)
    {
        finish(service, METER_CALIBRATION_ADMISSION_FAILED);
        return;
    }
    service->view.phase = METER_CALIBRATION_READING;
}
bool meter_calibration_take(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms,
                            meter_parameter_work_t *work)
{
    meter_calibration_step(service, profile, prerequisites, authorization, now_ms);
    if (!running(service) || !meter_parameters_take(&service->parameters, authorization, now_ms, work))
        return false;
    service->dispatched = true;
    return true;
}
bool meter_calibration_reply(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                             const meter_authorization_t *authorization, uint32_t now_ms,
                             const meter_parameter_reply_t *reply)
{
    meter_calibration_step(service, profile, prerequisites, authorization, now_ms);
    if (!running(service))
        return false;
    bool accepted = meter_parameters_reply(&service->parameters, reply, authorization, now_ms);
    meter_calibration_step(service, profile, prerequisites, authorization, now_ms);
    return accepted;
}
void meter_calibration_present(const meter_calibration_t *service, meter_calibration_view_t *out)
{
    *out = service->view;
}
bool meter_calibration_acknowledge(meter_calibration_t *service, uint32_t generation, uint64_t token)
{
    if (service->view.phase != METER_CALIBRATION_DONE || service->view.token != token ||
        service->view.profile_generation != generation)
        return false;
    service->view = (meter_calibration_view_t){0};
    return true;
}
