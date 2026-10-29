#include "contracts/hmi_firmware.h"
#include "contracts/hmi_product.h"
#include "core/hmi_core.h"
#include "core/hmi_snapshot.h"
#include "ui/common/i18n/hmi_i18n_runtime.h"
#include <assert.h>
#include <lvgl.h>
int main(void)
{
    hmi_firmware_composition_t composition;
    assert(!hmi_firmware_compose(NULL));
    assert(hmi_firmware_compose(&composition));
    const hmi_product_t *product = hmi_product_get();
    assert(product && product->ui && composition.locale_init);
    const hmi_core_storage_t stores[] = {composition.domain, composition.presentation, composition.ui};
    hmi_core_t cores[3];
    for (size_t i = 0u; i < 3u; ++i)
    {
        assert(hmi_core_init(&cores[i], product->catalog, &stores[i]));
        for (size_t j = 0u; j < i; ++j)
        {
            assert(!stores[i].signals || stores[i].signals != stores[j].signals);
            assert(!stores[i].parameters || stores[i].parameters != stores[j].parameters);
            assert(!stores[i].faults || stores[i].faults != stores[j].faults);
        }
    }
    assert(product->catalog->signal_count > 0u);
    cores[0].snapshot.signals[0].value = 17.0f;
    hmi_snapshot_t copy;
    for (size_t i = 1u; i < 3u; ++i)
    {
        assert(hmi_snapshot_copy(&copy, &stores[i], &cores[0].snapshot) == HMI_SNAPSHOT_COPIED);
        assert(copy.signals[0].value == 17.0f);
    }
    cores[0].snapshot.signals[0].value = 19.0f;
    assert(copy.signals[0].value == 17.0f);
    lv_init();
    assert(hmi_i18n_init() && composition.locale_init());
    lv_deinit();
    return 0;
}
