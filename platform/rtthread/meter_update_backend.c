#include "platform/rtthread/meter_update_backend.h"
/* 原型使用的 SDK 策略钩子已撤回；禁止以缺失的保护条件继续写入。 */
static meter_update_error_t begin(void *context, const meter_update_manifest_t *manifest)
{
    meter_aic_update_t *adapter = context;
    (void)manifest;
    if (adapter && adapter->rejected_sessions != UINT32_MAX)
        adapter->rejected_sessions++;
    return METER_UPDATE_UNSUPPORTED;
}
static meter_update_error_t write_block(void *context, const uint8_t *data, size_t size)
{
    (void)context;
    (void)data;
    (void)size;
    return METER_UPDATE_UNSUPPORTED;
}
static meter_update_error_t unavailable(void *context, const meter_update_manifest_t *manifest)
{
    (void)context;
    (void)manifest;
    return METER_UPDATE_UNSUPPORTED;
}
static void release(void *context)
{
    (void)context;
}
meter_update_backend_t meter_aic_update_backend(meter_aic_update_t *adapter)
{
    return (meter_update_backend_t){adapter, begin, write_block, unavailable, unavailable, release};
}
bool meter_aic_update_supported(void)
{
    return false;
}
const char *meter_aic_update_reason(void)
{
    return "sdk_candidate_validation_and_explicit_confirmation_unavailable";
}
int meter_aic_update_confirm(void)
{
    return -1;
}
