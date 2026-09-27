#include "sim/product_host.h"
#include "generated/can/mixed.h"
#include "canopen/mixed_canopen.h"
static void step(meter_runtime_t *r, uint32_t now)
{
    if (now % 96)
        return;
    meter_can_frame_t f = {.bus = METER_BUS_CAN0,
                           .id = MIXED_MOTION_FRAME_ID,
                           .timestamp_ms = now,
                           .extended = false,
                           .size = 8};
    struct mixed_motion_t motion = {.speed = mixed_motion_speed_encode(12.5f)};
    mixed_motion_pack(f.data, &motion, sizeof(f.data));
    meter_runtime_push(r, &f);
    f.id = MIXED_ENERGY_FRAME_ID;
    struct mixed_energy_t energy = {.soc = mixed_energy_soc_encode(82)};
    mixed_energy_pack(f.data, &energy, sizeof(f.data));
    meter_runtime_push(r, &f);
    /* CAN1 RPDO1: speed 6.5 m/s (65), torque -15 Nm (-30)，小端。 */
    meter_can_frame_t pdo = {.bus = METER_BUS_CAN1,
                             .id = 524,
                             .timestamp_ms = now,
                             .extended = false,
                             .size = 8};
    pdo.data[0] = 65;
    pdo.data[1] = 0;
    int16_t torque = -30;
    pdo.data[2] = (uint8_t)torque;
    pdo.data[3] = (uint8_t)(torque >> 8);
    meter_runtime_push(r, &pdo);
}
int main(int argc, char **argv)
{
    return meter_product_host(argc, argv, step);
}
