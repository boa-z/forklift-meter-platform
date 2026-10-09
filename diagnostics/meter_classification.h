#ifndef METER_CLASSIFICATION_H
#define METER_CLASSIFICATION_H
#include "contracts/meter_parameter.h"
/* 分类提供事件证据，不表示严重程度，也不请求全局健康状态迁移。
 * Product 或运行时策略根据事件上下文、持续性及关键程度决定健康状态。 */
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
