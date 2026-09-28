#ifndef METER_CLASSIFICATION_H
#define METER_CLASSIFICATION_H
#include "contracts/meter_parameter.h"
/* Classification is evidence, not severity or a requested global health transition.
 * Product/Runtime policy decides health using event context, persistence and criticality. */
typedef enum
{
    METER_EVENT_NONE,
    METER_EVENT_EXPECTED_SUPPRESSION,
    METER_EVENT_COMMUNICATION_STALE,
    METER_EVENT_REMOTE_REJECTION,
    METER_EVENT_TRANSACTION_TIMEOUT,
    METER_EVENT_TRANSPORT_FAILURE,
    METER_EVENT_RESOURCE_PRESSURE,
    METER_EVENT_PERSISTENCE_FAILURE,
    METER_EVENT_CONTRACT_VIOLATION
} meter_event_class_t;
meter_event_class_t meter_parameter_classify(meter_parameter_outcome_t outcome);
#endif
