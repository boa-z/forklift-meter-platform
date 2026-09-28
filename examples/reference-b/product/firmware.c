#include "catalog/catalog.h"
#include "contracts/meter_firmware.h"
/* Reference-B has four signals and no local Settings or fault records. */
enum
{
    SIGNAL_CAPACITY = 4
};
static meter_value_t domain[SIGNAL_CAPACITY], presentation[SIGNAL_CAPACITY];
static meter_value_t diagnostic[SIGNAL_CAPACITY], ui[SIGNAL_CAPACITY];
static bool locale_init(void)
{
    /* Reference-B's UI factory registers its own translation pack. */
    return true;
}
bool meter_firmware_compose(meter_firmware_composition_t *out)
{
    if (!out || product_catalog.signal_count > SIGNAL_CAPACITY || product_catalog.parameter_count != 0u ||
        product_catalog.fault_count != 0u)
        return false;
    *out = (meter_firmware_composition_t){
        .domain = {.signals = domain, .signal_capacity = SIGNAL_CAPACITY},
        .presentation = {.signals = presentation, .signal_capacity = SIGNAL_CAPACITY},
        .diagnostic = {.signals = diagnostic, .signal_capacity = SIGNAL_CAPACITY},
        .ui = {.signals = ui, .signal_capacity = SIGNAL_CAPACITY},
        .locale_init = locale_init};
    return true;
}
