#include "core/meter_core.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    static const meter_signal_def_t signal = {.id=19,.key="test.value",.unit="",.stale_ms=10};
    const meter_catalog_t catalog = {.signals=&signal,.signal_count=1};
    meter_value_t value = {.value=37};
    meter_core_storage_t storage = {.signals=&value,.signal_capacity=1};
    meter_core_t core = {0}, before = core;
    meter_initial_settings_t settings = {.language=METER_LANGUAGE_ZH,.can_rate=METER_CAN_RATE_250K,
                                         .brightness=40,.imperial=true};
    assert(meter_core_init_with_settings(&core,&catalog,&storage,&settings));
    assert(core.snapshot.language==METER_LANGUAGE_ZH && core.snapshot.can_rate==METER_CAN_RATE_250K);
    assert(core.snapshot.brightness==40 && core.snapshot.imperial);
    assert(value.state==METER_VALUE_UNKNOWN && core.snapshot.revision==0);
    before=core;
    value.value=37;
    for(unsigned i=0;i<4;++i) {
        meter_initial_settings_t invalid=settings;
        if(i==0) invalid.brightness=9;
        if(i==1) invalid.brightness=101;
        if(i==2) invalid.language=(meter_language_t)-1;
        if(i==3) invalid.can_rate=(meter_can_rate_t)3;
        assert(!meter_core_init_with_settings(&core,&catalog,&storage,&invalid));
        assert(memcmp(&core,&before,sizeof(core))==0 && value.value==37);
    }
    assert(!meter_core_init_with_settings(NULL,&catalog,&storage,&settings));
    assert(meter_core_init_with_settings(&core,&catalog,&storage,NULL));
    assert(core.snapshot.language==METER_LANGUAGE_EN && core.snapshot.can_rate==METER_CAN_RATE_500K);
    assert(core.snapshot.brightness==80 && !core.snapshot.imperial);
    return 0;
}
