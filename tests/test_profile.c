#include "core/meter_core.h"
#include "core/meter_snapshot.h"
#include <assert.h>
static const meter_signal_def_t signals[] = {{"sample", "V", 100, 1}};
static const meter_catalog_t catalog = {.signals = signals, .signal_count = 1};
/* Synthetic Product adaptation: raw identity/feature positions end here.
 * These values describe no customer controller or wire protocol. */
static bool publish_reference_profile(meter_core_t *core, unsigned identity, unsigned raw_features)
{
    if (identity != 0xA1u)
        return meter_core_profile(core, false, 0, 0);
    const uint64_t normalized = (raw_features & 0x40u) != 0u ? 8u : 0u;
    return meter_core_profile(core, true, 17, normalized);
}
int main(void)
{
    meter_core_t core;
    meter_value_t values[1], copies[1];
    meter_core_storage_t storage = {.signals = values, .signal_capacity = 1};
    meter_core_storage_t destination = {.signals = copies, .signal_capacity = 1};
    assert(meter_core_init(&core, &catalog, &storage));
    assert(!meter_profile_matches(&core.snapshot.profile, 0));
    assert(!meter_core_profile(&core, true, 0, 1));
    assert(!meter_core_profile(&core, false, 1, 0));
    assert(core.snapshot.revision == 0);
    assert(publish_reference_profile(&core, 0xA1u, 0x40u));
    assert(core.snapshot.profile.generation == 1 && core.snapshot.revision == 1);
    assert(publish_reference_profile(&core, 0xA1u, 0x40u));
    assert(core.snapshot.profile.generation == 1 && core.snapshot.revision == 1);
    meter_snapshot_t copy;
    assert(meter_snapshot_copy(&copy, &destination, &core.snapshot) == METER_SNAPSHOT_COPIED);
    assert(copy.profile.family == 17 && copy.profile.capabilities == 8);
    assert(meter_core_profile(&core, true, 17, 16));
    assert(!meter_profile_matches(&core.snapshot.profile, copy.profile.generation));
    assert(copy.profile.capabilities == 8);
    assert(publish_reference_profile(&core, 0xFFu, 0x40u));
    assert(core.snapshot.profile.generation == 3 && !core.snapshot.profile.confirmed);
    core.snapshot.profile.generation = UINT32_MAX;
    uint32_t revision = core.snapshot.revision;
    assert(!meter_core_profile(&core, true, 18, 0));
    assert(core.snapshot.revision == revision && !core.snapshot.profile.confirmed);
    return 0;
}
