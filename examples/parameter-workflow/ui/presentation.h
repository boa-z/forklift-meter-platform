#ifndef REFERENCE_PARAMETER_PRESENTATION_H
#define REFERENCE_PARAMETER_PRESENTATION_H
#include "contracts/meter_parameter.h"
/* UI-facing values only. No runtime, grant, transport, storage or mutable pointers. */
typedef struct
{
    uint32_t profile_generation;
    uint64_t view_token; /* UI allocates a new nonzero token for each panel lifetime. */
    meter_parameter_key_t key;
    meter_parameter_operation_t operation;
    float value;
} reference_parameter_intent_t;
typedef struct
{
    uint32_t profile_generation;
    uint64_t view_token;
    meter_request_id_t request;
    bool pending;
    bool has_result;
    meter_parameter_result_t result;
} reference_parameter_view_t;
/* A closed/replaced panel must not render an earlier panel's completion.
 * Ignoring a view is not an acknowledgement or backend cancellation. */
static inline bool reference_parameter_view_matches(const reference_parameter_view_t *view,
                                                    uint32_t generation, uint64_t token)
{
    return view && token != 0u && view->profile_generation == generation && view->view_token == token;
}
#endif
