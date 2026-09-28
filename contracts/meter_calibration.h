#ifndef METER_CALIBRATION_VIEW_H
#define METER_CALIBRATION_VIEW_H
#include "contracts/meter_domain.h"
#include "contracts/meter_parameter.h"
/* One App-owned capture/write/optional-readback, not a workflow engine.
 * Product supplies engineering-unit mapping, prerequisites and all transaction policy. */
typedef struct
{
    meter_signal_id_t source;
    meter_parameter_key_t target;
    uint32_t maximum_age_ms;
    bool verify;
    float tolerance;
    meter_parameter_policy_t write_policy, read_policy;
} meter_calibration_definition_t;
typedef enum
{
    METER_CALIBRATION_IDLE,
    METER_CALIBRATION_WRITING,
    METER_CALIBRATION_READING,
    METER_CALIBRATION_DONE
} meter_calibration_phase_t;
typedef enum
{
    METER_CALIBRATION_OK,
    METER_CALIBRATION_SOURCE_UNAVAILABLE,
    METER_CALIBRATION_PREREQUISITE,
    METER_CALIBRATION_PROFILE_CHANGED,
    METER_CALIBRATION_ADMISSION_FAILED,
    METER_CALIBRATION_TRANSACTION_FAILED,
    METER_CALIBRATION_VERIFY_MISMATCH
} meter_calibration_outcome_t;
typedef struct
{
    uint32_t profile_generation;
    uint64_t token;
    meter_calibration_phase_t phase;
    meter_calibration_outcome_t outcome; /* Meaningful only at DONE. */
    meter_parameter_admission_t admission;
    bool has_write_result, has_read_result, effect_unknown;
    meter_parameter_result_t write_result, read_result;
    float captured_value;
} meter_calibration_view_t;
static inline bool meter_calibration_view_matches(const meter_calibration_view_t *view,
                                                  const meter_profile_t *profile, uint64_t token)
{
    return view && token != 0u && view->token == token &&
           meter_profile_matches(profile, view->profile_generation);
}
#endif
