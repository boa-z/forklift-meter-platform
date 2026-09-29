#include "contracts/meter_firmware.h"
#include "product/demo_storage.h"
#include "ui/demo_i18n.h"
static demo_domain_store_t domain, presentation, diagnostic, ui;
bool meter_firmware_compose(meter_firmware_composition_t *out)
{
    if (!out)
        return false;
    *out = (meter_firmware_composition_t){.domain = demo_domain_bind(&domain),
                                          .presentation = demo_domain_bind(&presentation),
                                          .diagnostic = demo_domain_bind(&diagnostic),
                                          .ui = demo_domain_bind(&ui),
                                          .locale_init = demo_i18n_init};
    return true;
}
