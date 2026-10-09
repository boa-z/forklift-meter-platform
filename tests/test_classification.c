#include "diagnostics/meter_classification.h"
#include <assert.h>
int main(void)
{
    const meter_event_class_t expected[] = {METER_EVENT_NONE,
                                            METER_EVENT_TRANSACTION_TIMEOUT,
                                            METER_EVENT_EXPECTED_SUPPRESSION,
                                            METER_EVENT_EXPECTED_SUPPRESSION,
                                            METER_EVENT_REMOTE_REJECTION,
                                            METER_EVENT_TRANSPORT_FAILURE,
                                            METER_EVENT_TRANSPORT_FAILURE};
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        assert(meter_parameter_classify((meter_parameter_outcome_t)i) == expected[i]);
    assert(meter_parameter_classify((meter_parameter_outcome_t)99) == METER_EVENT_CONTRACT_VIOLATION);
    /* 映射器不接受运行时或健康状态对象，不修改状态，也不调用输出后端。 */
    return 0;
}
