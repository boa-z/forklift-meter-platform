#include "ui/ui.h"
#include "catalog/catalog.h"
#include "platform/host/host_platform.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void)
{
    meter_value_t values[4];
    meter_core_storage_t storage={.signals=values,.signal_capacity=4};
    meter_core_t core;
    CHECK(meter_core_init(&core,&product_catalog,&storage));
    CHECK(!meter_product_get()->capabilities->weighing);
    lv_init(); CHECK(meter_i18n_init()); CHECK(meter_host_open(true));
    void *ui=product_ui.create(lv_screen_active(),NULL); CHECK(ui);
    for(unsigned lang=0;lang<2;++lang)
    {
        core.snapshot.language=(meter_language_t)lang;
        for(unsigned frame=0;frame<15;++frame)
        {
            if(frame==2 || frame==5) meter_host_click(lang ? 200:600,32,frame==2);
            CHECK(meter_host_events());
            product_ui.present(ui,&core.snapshot,16);
            lv_tick_inc(16);lv_timer_handler();
        }
        CHECK(reference_b_page(ui)==(lang ? 0u:1u));
    }
    product_ui.destroy(ui);meter_host_close();lv_deinit();
    return 0;
}
