#ifndef METER_CORE_H
#define METER_CORE_H
#include "contracts/meter_domain.h"
typedef struct
{
    meter_snapshot_t snapshot;
    const meter_catalog_t *catalog;
} meter_core_t;
bool meter_core_init(meter_core_t *core, const meter_catalog_t *catalog);
bool meter_core_apply(void *context, const meter_update_t *update);
void meter_core_tick(meter_core_t *core, uint32_t now_ms, uint32_t stale_ms);
void meter_core_connection(meter_core_t *core, bool connected, uint32_t generation);
bool meter_core_action(meter_core_t *core, const meter_action_t *action);
bool meter_core_parameter(meter_core_t *core, uint16_t id, float value);
const meter_snapshot_t *meter_core_snapshot(const meter_core_t *core);
#endif
