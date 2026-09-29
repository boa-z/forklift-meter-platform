#include "application/presentation.h"
#include "core/meter_core.h"
#include "product/demo_storage.h"
#include <assert.h>
#include <math.h>
int main(void)
{
    demo_domain_store_t storage;
    meter_core_storage_t binding = demo_domain_bind(&storage);
    meter_core_t core;
    assert(meter_core_init(&core, &meter_demo_catalog, &binding));
    demo_presentation_t view;
    demo_presentation_build(&core.snapshot, &view);
    assert(view.speed.state == METER_VALUE_UNKNOWN && view.link == DEMO_LINK_OFFLINE);
    meter_core_connection(&core, true, 1);
    demo_presentation_build(&core.snapshot, &view);
    assert(view.link == DEMO_LINK_WAITING);
    meter_update_t update = {METER_SPEED, {10, 1, METER_VALUE_VALID, METER_SOURCE_DEMO}};
    assert(meter_core_apply(&core, &update));
    assert(meter_core_action(&core, &(meter_action_t){METER_ACTION_UNITS, 0, 1}));
    demo_presentation_build(&core.snapshot, &view);
    assert(fabsf(view.speed.value - 6.21371f) < 0.0001f && view.speed.state == METER_VALUE_VALID);
    assert(view.speed_maximum == 32 && view.link == DEMO_LINK_CONNECTED);
    update.value.state = METER_VALUE_STALE;
    assert(meter_core_apply(&core, &update));
    assert(view.speed.state == METER_VALUE_VALID);
    demo_presentation_build(&core.snapshot, &view);
    assert(view.speed.state == METER_VALUE_STALE && view.link == DEMO_LINK_STALE);
    update.value.state = METER_VALUE_ERROR;
    assert(meter_core_apply(&core, &update));
    demo_presentation_build(&core.snapshot, &view);
    assert(view.speed.state == METER_VALUE_ERROR);
    meter_action_t intent = demo_speed_limit_intent(30);
    assert(view.limit != 30 && meter_core_action(&core, &intent));
    demo_presentation_build(&core.snapshot, &view);
    assert(view.limit_available && view.limit == 30);
    assert(meter_snapshot_fault_set(&core.snapshot, DEMO_FAULT_LOW_CHARGE, true));
    demo_presentation_build(&core.snapshot, &view);
    assert(view.faults[0].active && view.faults[0].code == DEMO_FAULT_LOW_CHARGE);
    assert(meter_core_profile(&core, true, 1, 4));
    demo_presentation_build(&core.snapshot, &view);
    assert(meter_profile_matches(&view.profile, core.snapshot.profile.generation));
    assert(meter_core_profile(&core, true, 2, 0));
    assert(!meter_profile_matches(&core.snapshot.profile, view.profile.generation));
    /* 根据当前快照副本重新构建，防止旧系列配置被当作当前配置。 */
    demo_presentation_build(&core.snapshot, &view);
    assert(view.profile.family == 2 && view.profile.capabilities == 0);
    return 0;
}
