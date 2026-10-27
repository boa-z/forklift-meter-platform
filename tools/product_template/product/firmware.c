#include "catalog/catalog.h"
#include "contracts/meter_firmware.h"
#include "ui/ui.h"

/* 运行时 Domain、发给 UI 的发布副本、UI 自己的副本各一份；扩展目录时同步调整容量，不共享可变数组。 */
enum { PRODUCT_SIGNAL_CAPACITY = 1 };
static meter_value_t domain[PRODUCT_SIGNAL_CAPACITY], presentation[PRODUCT_SIGNAL_CAPACITY],
    ui[PRODUCT_SIGNAL_CAPACITY];

bool meter_firmware_compose(meter_firmware_composition_t *out)
{
    if (!out || product_catalog.signal_count > PRODUCT_SIGNAL_CAPACITY ||
        product_catalog.parameter_count != 0u || product_catalog.fault_count != 0u)
        return false;
    *out = (meter_firmware_composition_t){
        .domain = {.signals = domain, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .presentation = {.signals = presentation, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .ui = {.signals = ui, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .locale_init = product_locale_init};
    return true;
}
