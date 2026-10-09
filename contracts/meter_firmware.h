#ifndef METER_FIRMWARE_H
#define METER_FIRMWARE_H
#include "contracts/meter_domain.h"
/** Build-time Product bootstrap. Called once before owners start; no device I/O.
 * Each storage view has independent, static backing for its owner. Locale setup
 * runs on the UI owner after platform i18n init and before UI creation.
 * A selected Product supplies exactly one implementation, never a runtime loader. */
typedef struct
{
    meter_core_storage_t domain, presentation, diagnostic, ui;
    bool (*locale_init)(void);
} meter_firmware_composition_t;
bool meter_firmware_compose(meter_firmware_composition_t *out);
#endif
