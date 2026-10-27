#ifndef METER_FIRMWARE_H
#define METER_FIRMWARE_H
#include "contracts/meter_domain.h"
/** Build-time Product bootstrap. Called once before the runtime starts; no device I/O.
 * domain backs the runtime-owned Core, presentation the copy published to the UI and ui the UI
 * thread's private copy; the three must not overlap. Locale setup runs on the UI thread after
 * platform i18n init and before UI creation. A selected Product supplies exactly one
 * implementation, never a runtime loader. */
typedef struct
{
    meter_core_storage_t domain, presentation, ui;
    bool (*locale_init)(void);
} meter_firmware_composition_t;
bool meter_firmware_compose(meter_firmware_composition_t *out);
#endif
