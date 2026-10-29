#include "catalog/catalog.h"
#include "i18n/generated/product_i18n.h"
/* 信号身份由本 Product 分配；本地算出的值与协议解码的值放在同一张表里。 */
static const hmi_signal_def_t signals[] = {
    {.id = PRODUCT_UPTIME, .key = "app.uptime", .unit = "s"},
    {.id = PRODUCT_SPEED, .key = "telemetry.speed", .unit = "m/s", .stale_ms = 500}, /* [can] */
};
const hmi_catalog_t product_catalog = {.signals = signals,
                                       .signal_count = sizeof(signals) / sizeof(signals[0]),
                                       .language_count = PRODUCT_I18N_LANGUAGE_COUNT};
