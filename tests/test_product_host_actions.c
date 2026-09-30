#include "sim/product_host.h"
#include <assert.h>
static unsigned calls;
static bool accept;
static meter_snapshot_t *received;
static uint32_t sampled;
static bool local(meter_snapshot_t *snapshot, const meter_action_t *request, uint32_t now)
{
    ++calls; received=snapshot; sampled=now;
    assert(request->id==17u && request->value==2.0f);
    return accept;
}
int main(void)
{
    meter_core_t core={0};
    meter_product_t product={.local_action=local};
    meter_action_t request={.kind=METER_ACTION_PRODUCT,.id=17u,.value=2.0f};
    assert(!meter_product_host_action(NULL,&product,&request,10u));
    assert(!meter_product_host_action(&core,NULL,&request,10u));
    assert(!meter_product_host_action(&core,&product,NULL,10u));
    assert(calls==0u);
    assert(!meter_product_host_action(&core,&product,&request,10u));
    assert(calls==1u && received==&core.snapshot && sampled==10u);
    accept=true;
    assert(meter_product_host_action(&core,&product,&request,20u));
    assert(calls==2u && sampled==20u);
    product.local_action=NULL;
    assert(!meter_product_host_action(&core,&product,&request,30u));
    assert(calls==2u);
    request.kind=METER_ACTION_BRIGHTNESS;request.value=80.0f;
    assert(meter_product_host_action(&core,&product,&request,40u));
    assert(core.snapshot.brightness==80u && calls==2u);
    return 0;
}
