#include "diagnostics/meter_classification.h"
meter_event_class_t meter_parameter_classify(meter_parameter_outcome_t outcome)
{
    switch (outcome)
    {
    case METER_PARAMETER_SUCCEEDED:
        return METER_EVENT_NONE;
    case METER_PARAMETER_PERMISSION_LOST:
    case METER_PARAMETER_CANCELLED:
        return METER_EVENT_EXPECTED_SUPPRESSION;
    case METER_PARAMETER_REMOTE_REJECTED:
        return METER_EVENT_REMOTE_REJECTION;
    case METER_PARAMETER_TIMED_OUT:
        return METER_EVENT_TRANSACTION_TIMEOUT;
    case METER_PARAMETER_TRANSPORT_FAILED:
        return METER_EVENT_TRANSPORT_FAILURE;
    /* A malformed remote response is transport evidence, not proof of a local invariant failure. */
    case METER_PARAMETER_INVALID_REPLY:
        return METER_EVENT_TRANSPORT_FAILURE;
    default:
        return METER_EVENT_CONTRACT_VIOLATION;
    }
}
