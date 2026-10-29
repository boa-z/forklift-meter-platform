#include "catalog/catalog.h"
#include "i18n/generated/product_i18n.h"
static const hmi_signal_def_t signals[] = {{.id = PRODUCT_SPEED, .key = "vehicle.speed", .unit = "m/s", .stale_ms = 500}};
const hmi_catalog_t product_catalog = {.signals=signals,.signal_count=1,
    .language_count=PRODUCT_I18N_LANGUAGE_COUNT};
