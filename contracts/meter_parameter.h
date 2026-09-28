#ifndef METER_PARAMETER_H
#define METER_PARAMETER_H
#include "contracts/meter_request.h"

/** Logical Product identity, not a CAN ID, SDO address or persisted setting offset. */
typedef struct
{
    uint16_t owner; /* Nonzero logical endpoint. */
    uint16_t id;
} meter_parameter_key_t;

static inline bool meter_parameter_key_equal(meter_parameter_key_t first, meter_parameter_key_t second)
{
    return (first.owner == second.owner) && (first.id == second.id);
}

typedef enum
{
    METER_PARAMETER_READ,
    METER_PARAMETER_WRITE
} meter_parameter_operation_t;

/** Immutable Product catalog. Zero initialization is deliberately unconfirmed.
 * Numeric values use engineering-unit floats; codecs own exact wire representation.
 * Values requiring lossless wide integers/blobs need a separate reviewed extension. */
typedef struct
{
    meter_parameter_key_t key;
    bool confirmed;
    bool readable;
    bool writable;
    bool repeatable_read; /* Product confirms retries cannot repeat side effects. */
    float minimum;
    float maximum;
    uint32_t read_permissions;
    uint32_t write_permissions; /* Must be nonzero for a confirmed writable entry. */
} meter_parameter_definition_t;

typedef struct
{
    uint32_t total_timeout_ms;   /* From admission, including queued time. */
    uint32_t attempt_timeout_ms; /* From handing a copied request to the backend. */
    uint8_t max_attempts;        /* Writes always require one; reads may opt into retry. */
} meter_parameter_policy_t;

typedef struct
{
    meter_request_id_t id;
    meter_parameter_key_t key;
    meter_parameter_operation_t operation;
    float value;     /* Meaningful only for WRITE. */
    uint8_t attempt; /* One-based; echoed by a validating backend. */
} meter_parameter_work_t;

typedef enum
{
    METER_PARAMETER_REPLY_OK,
    METER_PARAMETER_REPLY_REJECTED,
    METER_PARAMETER_REPLY_TRANSPORT_FAILED
} meter_parameter_reply_code_t;

typedef struct
{
    meter_parameter_work_t request;
    meter_parameter_reply_code_t code;
    bool has_value;
    float value;
    int32_t detail; /* Backend-specific diagnostic, never a health mode. */
} meter_parameter_reply_t;

typedef enum
{
    METER_PARAMETER_SUCCEEDED,
    METER_PARAMETER_TIMED_OUT,
    METER_PARAMETER_PERMISSION_LOST,
    METER_PARAMETER_CANCELLED,
    METER_PARAMETER_REMOTE_REJECTED,
    METER_PARAMETER_INVALID_REPLY,
    METER_PARAMETER_TRANSPORT_FAILED
} meter_parameter_outcome_t;

typedef struct
{
    meter_parameter_work_t request;
    meter_parameter_outcome_t outcome;
    bool effect_unknown; /* Dispatched write may have taken effect; no rollback claim. */
    bool has_value;
    float value;
    int32_t detail;
} meter_parameter_result_t;

typedef enum
{
    METER_PARAMETER_ACCEPTED,
    METER_PARAMETER_BUSY,
    METER_PARAMETER_INVALID,
    METER_PARAMETER_NOT_FOUND,
    METER_PARAMETER_UNCONFIRMED,
    METER_PARAMETER_DENIED,
    METER_PARAMETER_OUT_OF_RANGE,
    METER_PARAMETER_EXHAUSTED
} meter_parameter_admission_t;
#endif
