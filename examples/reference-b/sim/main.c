#include "sim/product_host.h"
#include "generated/can/product.h"
static void step(meter_runtime_t *r,uint32_t now)
{
    if(now%96) return;
    meter_can_frame_t f={.bus=METER_BUS_CAN1,.id=PRODUCT_DRIVETRAIN_FRAME_ID,.timestamp_ms=now,.extended=true,.size=8};
    struct product_drivetrain_t drive={.velocity=product_drivetrain_velocity_encode((now%12000)/1000.0f),
        .torque=product_drivetrain_torque_encode(-15)};
    product_drivetrain_pack(f.data,&drive,sizeof(f.data));
    meter_runtime_push(r,&f);
    f.id=PRODUCT_POWER_FRAME_ID;
    struct product_power_t power={.remaining=product_power_remaining_encode(82),.ambient=product_power_ambient_encode(23)};
    product_power_pack(f.data,&power,sizeof(f.data));
    meter_runtime_push(r,&f);
}
int main(int argc,char **argv) { return meter_product_host(argc,argv,step); }
