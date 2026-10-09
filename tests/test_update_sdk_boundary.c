#include "platform/rtthread/meter_update_backend.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    meter_aic_update_t adapter = {0};
    meter_update_backend_t backend = meter_aic_update_backend(&adapter);
    meter_update_policy_t policy = {"synthetic", "reference-board", 4096, 1000};
    meter_update_manifest_t manifest = {
        .product = "synthetic", .hardware = "reference-board", .version = "v2", .size = 4};
    meter_firmware_update_t service;
    uint8_t bytes[4] = {0};
    /* 实际后端不链接 SDK，任何会话都不得降级为未经保护的 Flash 写入。 */
    assert(!meter_aic_update_supported());
    assert(strlen(meter_aic_update_reason()) > 0);
    assert(meter_aic_update_confirm() == -1);
    assert(meter_update_init(&service, &policy, &backend));
    assert(meter_update_begin(&service, &manifest, true, 0) == METER_UPDATE_UNSUPPORTED);
    assert(service.state == METER_UPDATE_FAILED && !service.opened);
    assert(adapter.rejected_sessions == 1);
    assert(meter_update_write(&service, service.generation, 0, bytes, 4, 1) == METER_UPDATE_STATE);
    assert(meter_update_activate(&service, service.generation, true) == METER_UPDATE_STATE);
    assert(backend.write(backend.context, bytes, 4) == METER_UPDATE_UNSUPPORTED);
    assert(backend.verify(backend.context, &manifest) == METER_UPDATE_UNSUPPORTED);
    assert(backend.activate(backend.context, &manifest) == METER_UPDATE_UNSUPPORTED);
    backend.abort(backend.context);
    assert(meter_update_begin(&service, &manifest, true, 2) == METER_UPDATE_UNSUPPORTED);
    assert(adapter.rejected_sessions == 2);
    return 0;
}
