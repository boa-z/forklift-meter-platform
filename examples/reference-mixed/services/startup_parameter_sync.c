#include "startup_parameter_sync.h"
#include <string.h>
void mixed_startup_reset(mixed_startup_sync_t *s)
{
    memset(s, 0, sizeof(*s));
}
static bool read_parameter(meter_sdo_channel_t *sdo, unsigned item)
{
    meter_sdo_request_t r = {.request_id = 1 + item,
                             .node_id = 12,
                             .index = 0x2000,
                             .subindex = (uint8_t)(1 + item),
                             .operation = METER_SDO_READ,
                             .size = 2,
                             .timeout_ms = 120,
                             .retry_count = 3,
                             .retry_delay_ms = 120};
    return meter_sdo_submit(sdo, &r);
}
bool mixed_startup_process(mixed_startup_sync_t *s, meter_sdo_channel_t *sdo, bool ready)
{
    if (!s || !sdo || !ready)
        return false;
    if (s->phase == MIXED_SYNC_WAIT_DATA)
    {
        if (read_parameter(sdo, 0))
            s->phase = MIXED_SYNC_READ_A;
        return false;
    }
    if (s->phase != MIXED_SYNC_READ_A && s->phase != MIXED_SYNC_READ_B)
        return false;
    unsigned item = s->phase == MIXED_SYNC_READ_A ? 0 : 1;
    meter_sdo_result_t result;
    if (!meter_sdo_take(sdo, 1 + item, &result))
        return false;
    if (result.status != METER_SDO_SUCCESS || result.size != 2)
    {
        s->phase = MIXED_SYNC_FAILED;
        s->error = result.abort_code ? result.abort_code : 1;
        return false;
    }
    /* 这里只解释参数值载荷，传输分段和协议控制字节全部由上游处理。 */
    s->parameters[item] = (uint16_t)result.payload[0] | ((uint16_t)result.payload[1] << 8);
    if (item == 1)
    {
        s->phase = MIXED_SYNC_READY;
        return true;
    }
    if (read_parameter(sdo, 1))
        s->phase = MIXED_SYNC_READ_B;
    else
    {
        s->phase = MIXED_SYNC_FAILED;
        s->error = 2;
    }
    return false;
}
