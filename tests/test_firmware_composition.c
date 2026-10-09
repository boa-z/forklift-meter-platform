#include "contracts/meter_firmware.h"
#include "contracts/meter_product.h"
#include "core/meter_core.h"
#include "core/meter_snapshot.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <assert.h>
#include <lvgl.h>
int main(void)
{
    meter_firmware_composition_t composition;
    assert(!meter_firmware_compose(NULL));
    assert(meter_firmware_compose(&composition));
    const meter_product_t *product = meter_product_get();
    assert(product && product->ui && composition.locale_init);
    const meter_core_storage_t stores[] = {composition.domain, composition.presentation,
                                           composition.diagnostic, composition.ui};
    meter_core_t cores[4];
    for (size_t i = 0u; i < 4u; ++i)
    {
        assert(meter_core_init(&cores[i], product->catalog, &stores[i]));
        for (size_t j = 0u; j < i; ++j)
        {
            assert(!stores[i].signals || stores[i].signals != stores[j].signals);
            assert(!stores[i].parameters || stores[i].parameters != stores[j].parameters);
            assert(!stores[i].faults || stores[i].faults != stores[j].faults);
        }
    }
    assert(product->catalog->signal_count > 0u);
    cores[0].snapshot.signals[0].value = 17.0f;
    meter_snapshot_t copy;
    for (size_t i = 1u; i < 4u; ++i)
    {
        assert(meter_snapshot_copy(&copy, &stores[i], &cores[0].snapshot) == METER_SNAPSHOT_COPIED);
        assert(copy.signals[0].value == 17.0f);
    }
    cores[0].snapshot.signals[0].value = 19.0f;
    assert(copy.signals[0].value == 17.0f);
    lv_init();
    assert(meter_i18n_init() && composition.locale_init());
    lv_deinit();
    return 0;
}
