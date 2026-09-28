#ifndef REFERENCE_PARAMETER_APP_H
#define REFERENCE_PARAMETER_APP_H
#include "contracts/meter_profile.h"
#include "examples/parameter-workflow/ui/presentation.h"
#include "runtime/meter_parameters.h"
/* Nonblocking copied-message endpoints, called by App. A production port posts
 * to its protocol owner; it must not perform I/O or run another owner inline.
 * ready includes backend drain/quarantine, independently of App result credit. */
typedef struct
{
    void *context;
    bool (*ready)(void *context);
    bool (*send)(void *context, const meter_parameter_work_t *work);
    bool (*receive)(void *context, meter_parameter_reply_t *reply);
} reference_parameter_port_t;
typedef struct
{
    meter_parameters_t parameters;
    meter_authorization_t authorization;
    meter_parameter_policy_t policy;
    reference_parameter_view_t view;
    uint32_t generation;
    bool active;
    bool profile_ready;
} reference_parameter_app_t;
/* Serialized App-only API. Pass initialized, nonnull objects and valid port callbacks.
 * Inputs/outputs must not alias App state. Do not copy a live instance. No new worker/locks.
 * UI sends intents via its existing copied IPC; never pass it this object.
 * session and profile generation are nonzero, caller-managed lifetime tokens. */
bool reference_parameter_app_init(reference_parameter_app_t *app, const meter_parameter_definition_t *catalog,
                                  size_t count, uint32_t session, uint32_t generation,
                                  meter_parameter_policy_t policy);
meter_parameter_admission_t reference_parameter_app_submit(reference_parameter_app_t *app,
                                                           const reference_parameter_intent_t *intent,
                                                           uint32_t now_ms);
void reference_parameter_app_step(reference_parameter_app_t *app, const reference_parameter_port_t *port,
                                  uint32_t now_ms);
bool reference_parameter_app_cancel(reference_parameter_app_t *app, uint64_t token, meter_request_id_t id);
bool reference_parameter_app_acknowledge(reference_parameter_app_t *app, uint64_t token,
                                         meter_request_id_t id);
/* App copies into publication IPC; UI receives its own value, never an alias. */
void reference_parameter_app_present(const reference_parameter_app_t *app, reference_parameter_view_t *out);
/* Reference policy only: profile replacement loses caller interest and revokes its grant.
 * Keeps original token/generation/result until acknowledged; backend still owns drain.
 * No production safety policy is selected by this example. Reject rollback/reused epochs. */
bool reference_parameter_app_profile(reference_parameter_app_t *app, const meter_profile_t *profile);
#endif
