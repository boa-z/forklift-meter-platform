#include "catalog/catalog.h"
#include "contracts/meter_firmware.h"

/* 每个 owner 独立存储；扩展目录时同步调整容量，不共享可变数组。 */
enum { PRODUCT_SIGNAL_CAPACITY = 1 };
static meter_value_t domain[PRODUCT_SIGNAL_CAPACITY], presentation[PRODUCT_SIGNAL_CAPACITY];
static meter_value_t diagnostic[PRODUCT_SIGNAL_CAPACITY], ui[PRODUCT_SIGNAL_CAPACITY];

static bool locale_init(void)
{
    /* 自定义翻译包可在此注册；最小模板只使用公共词汇。 */
    return true;
}

bool meter_firmware_compose(meter_firmware_composition_t *out)
{
    if (!out || product_catalog.signal_count > PRODUCT_SIGNAL_CAPACITY ||
        product_catalog.parameter_count != 0u || product_catalog.fault_count != 0u)
        return false;
    *out = (meter_firmware_composition_t){
        .domain = {.signals = domain, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .presentation = {.signals = presentation, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .diagnostic = {.signals = diagnostic, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .ui = {.signals = ui, .signal_capacity = PRODUCT_SIGNAL_CAPACITY},
        .locale_init = locale_init};
    return true;
}
