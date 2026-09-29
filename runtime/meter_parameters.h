#ifndef METER_PARAMETERS_H
#define METER_PARAMETERS_H
#include "contracts/meter_parameter.h"
#include "runtime/meter_authorization.h"
#include "runtime/meter_requests.h"

/** One App-owned transaction and retained result per service. No heap, locks, I/O or callbacks.
 * Fields are implementation state. Do not copy/move an initialized service (ledger points to slot). */
typedef struct
{
    const meter_parameter_definition_t *catalog;
    size_t count;
    meter_requests_t ledger;
    meter_request_slot_t slot;
    const meter_parameter_definition_t *definition;
    meter_parameter_work_t work;
    meter_parameter_policy_t policy;
    meter_parameter_result_t result;
    uint32_t total_deadline_ms;
    uint32_t attempt_deadline_ms;
    uint32_t permissions;
    uint64_t authorization_epoch;
    bool waiting;
} meter_parameters_t;

/** Bind immutable catalog for the service lifetime; reject duplicate owner+ID and malformed
 * confirmed descriptors. Nonzero session must not be reused while old replies may arrive.
 * Call once before use, never on a live instance. Failure leaves destination unchanged. */
bool meter_parameters_init(meter_parameters_t *service, const meter_parameter_definition_t *catalog,
                           size_t count, uint32_t session);
/** All calls serialized by App. authorization is the same logical grant throughout each request.
 * Caller inputs/outputs must not alias service or catalog. Failure leaves id untouched. */
meter_parameter_admission_t meter_parameters_submit(meter_parameters_t *service, meter_parameter_key_t key,
                                                    meter_parameter_operation_t operation, float value,
                                                    meter_parameter_policy_t policy,
                                                    const meter_authorization_t *authorization,
                                                    uint32_t now_ms, meter_request_id_t *id);
/** Process permission loss and timeouts. Call within half a monotonic clock cycle of deadlines. */
void meter_parameters_tick(meter_parameters_t *service, const meter_authorization_t *authorization,
                           uint32_t now_ms);
/** Copy next attempt to backend; this is the irreversible dispatch boundary.
 * Backend must serialize/quarantine ambiguous late wire replies before accepting another attempt.
 * Local correlation tokens alone cannot disambiguate an untagged wire protocol. */
bool meter_parameters_take(meter_parameters_t *service, const meter_authorization_t *authorization,
                           uint32_t now_ms, meter_parameter_work_t *out);
/** Wrong ID/key/operation/attempt is ignored. A correlated malformed reply terminates the request.
 * Backend validates bus/source/wire format before constructing this value message. */
bool meter_parameters_reply(meter_parameters_t *service, const meter_parameter_reply_t *reply,
                            const meter_authorization_t *authorization, uint32_t now_ms);
/** Cancel caller interest. Dispatched writes finish with effect_unknown; backend I/O is not stopped. */
bool meter_parameters_cancel(meter_parameters_t *service, meter_request_id_t id);
/** Copy retained terminal result; caller polling/IPC notification remains App integration. */
bool meter_parameters_query(const meter_parameters_t *service, meter_request_id_t id,
                            meter_parameter_result_t *out);
/** Free only result credit. Does not free or drain backend resources. */
bool meter_parameters_acknowledge(meter_parameters_t *service, meter_request_id_t id);
#endif
