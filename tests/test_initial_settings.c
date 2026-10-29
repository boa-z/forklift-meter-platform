#include "core/hmi_core.h"
#include "core/hmi_settings.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    static const hmi_signal_def_t signal = {.id=19,.key="test.value",.unit="",.stale_ms=10};
    const hmi_catalog_t catalog = {.signals=&signal,.signal_count=1};
    hmi_value_t value = {.value=37};
    hmi_core_storage_t storage = {.signals=&value,.signal_capacity=1};
    hmi_core_t core = {0}, before = core;
    hmi_initial_settings_t settings = {.language=HMI_LANGUAGE_ZH,.can_rate=HMI_CAN_RATE_250K,
                                         .brightness=40,.imperial=true};
    assert(hmi_core_init_with_settings(&core,&catalog,&storage,&settings));
    assert(core.snapshot.language==HMI_LANGUAGE_ZH && core.snapshot.can_rate==HMI_CAN_RATE_250K);
    assert(core.snapshot.brightness==40 && core.snapshot.imperial);
    assert(value.state==HMI_VALUE_UNKNOWN && core.snapshot.revision==0);
    before=core;
    value.value=37;
    for(unsigned i=0;i<4;++i) {
        hmi_initial_settings_t invalid=settings;
        if(i==0) invalid.brightness=9;
        if(i==1) invalid.brightness=101;
        if(i==2) invalid.language=(hmi_language_t)-1;
        if(i==3) invalid.can_rate=(hmi_can_rate_t)3;
        assert(!hmi_core_init_with_settings(&core,&catalog,&storage,&invalid));
        assert(memcmp(&core,&before,sizeof(core))==0 && value.value==37);
    }
    /* 语言数由目录声明：默认只有内置的 EN/ZH，产品声明 3 种后第 3 种（ID 2）才被接受；
       动作与持久化记录用同一个上限。 */
    {
        hmi_initial_settings_t third = settings;
        third.language = (hmi_language_t)2;
        assert(!hmi_core_init_with_settings(&core, &catalog, &storage, &third));
        hmi_catalog_t trilingual = catalog;
        trilingual.language_count = 3;
        assert(hmi_core_init_with_settings(&core, &trilingual, &storage, &third));
        assert((unsigned)core.snapshot.language == 2u);
        hmi_action_t action = {.kind = HMI_ACTION_LANGUAGE, .value = 2.0f};
        assert(hmi_core_action(&core, &action));
        action.value = 3.0f;
        assert(!hmi_core_action(&core, &action));
        action.value = 1.5f;
        assert(!hmi_core_action(&core, &action));
        action.value = -1.0f;
        assert(!hmi_core_action(&core, &action));
        assert(hmi_core_init_with_settings(&core, &catalog, &storage, &settings));
        action.value = 2.0f;
        assert(!hmi_core_action(&core, &action));
        action.value = 1.0f;
        assert(hmi_core_action(&core, &action));
    }
    assert(!hmi_core_init_with_settings(NULL,&catalog,&storage,&settings));
    /* 单语言 Product 只接受 ID 0；来自旧双语言记录的 ID 1 不可越过目录边界。 */
    {
        uint8_t saved[64];
        assert(hmi_settings_encode(&core, saved, sizeof(saved)));
        size_t saved_size = hmi_settings_size(&core);
        hmi_catalog_t single = catalog;
        single.language_count = 1;
        assert(!hmi_core_init_with_settings(&core, &single, &storage, &settings));
        assert(hmi_core_init_with_settings(&core, &single, &storage, NULL));
        hmi_action_t action = {.kind = HMI_ACTION_LANGUAGE, .value = 1.0f};
        assert(!hmi_core_action(&core, &action));
        assert(!hmi_settings_decode(&core, saved, saved_size));
        assert(core.snapshot.language == 0);
        action.value = 0.0f;
        assert(hmi_core_action(&core, &action));
    }
    assert(hmi_core_init_with_settings(&core,&catalog,&storage,NULL));
    assert(core.snapshot.language==HMI_LANGUAGE_EN && core.snapshot.can_rate==HMI_CAN_RATE_500K);
    assert(core.snapshot.brightness==80 && !core.snapshot.imperial);
    return 0;
}
