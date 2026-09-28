#ifndef METER_CALIBRATION_H
#define METER_CALIBRATION_H
#include "contracts/meter_calibration.h"
#include "runtime/meter_parameters.h"
typedef struct
{
    meter_parameters_t parameters;
    meter_calibration_definition_t definition;
    meter_calibration_view_t view;
    meter_request_id_t request;
    uint32_t captured_ms;
    bool dispatched;
} meter_calibration_t;
/* App serialized. Init once; never copy a live service. Output/input cannot alias state.
 * After successful init, step/take/reply/present/acknowledge require nonnull pointers
 * and this initialized service. Product must supply the current coherent profile. */
bool meter_calibration_init(meter_calibration_t *service, const meter_parameter_definition_t *catalog,
                            size_t count, uint32_t session);
/* False: malformed arguments or retained workflow; state unchanged.
 * True: run admitted, possibly immediately DONE with typed denial. No wire dispatch.
 * measurement_generation attests the source was acquired for that profile; Product must
 * not stamp cached old-controller values with a new generation. snapshot is consistent.
 * token is a non-reused panel/workflow lifetime identity, not an authorization grant. */
bool meter_calibration_begin(meter_calibration_t *service, const meter_calibration_definition_t *definition,
                             const meter_snapshot_t *snapshot, uint32_t measurement_generation,
                             uint64_t token, bool prerequisites, const meter_authorization_t *authorization,
                             uint32_t now_ms);
void meter_calibration_step(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms);
/* Call only when backend has independent drain/quarantine credit. Copies one request. */
bool meter_calibration_take(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                            const meter_authorization_t *authorization, uint32_t now_ms,
                            meter_parameter_work_t *work);
bool meter_calibration_reply(meter_calibration_t *service, const meter_profile_t *profile, bool prerequisites,
                             const meter_authorization_t *authorization, uint32_t now_ms,
                             const meter_parameter_reply_t *reply);
void meter_calibration_present(const meter_calibration_t *service, meter_calibration_view_t *out);
bool meter_calibration_acknowledge(meter_calibration_t *service, uint32_t generation, uint64_t token);
#endif
