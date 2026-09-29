#ifndef DEMO_PDO_H
#define DEMO_PDO_H
#include "contracts/meter_periodic.h"
/** @brief Demo App 与 Protocol 共用的语义槽位；单位见采样端，非 Domain 身份。 */
enum
{
    DEMO_TX_BRIGHTNESS,
    DEMO_TX_SPEED,
    DEMO_TX_SOC,
    DEMO_TX_HEIGHT,
    DEMO_TX_LOAD,
    DEMO_TX_SEAT,
    DEMO_TX_BRAKE,
    DEMO_TX_NEUTRAL,
    DEMO_TX_CHARGING,
    DEMO_TX_WARNING,
    DEMO_TX_VALUE_COUNT
};
#define DEMO_PDO_MOTION_ID 0x381u
#define DEMO_PDO_STATUS_ID 0x481u
/** @brief 仅编码公开合成周期数据，不实现 CANopen 状态机或客户协议。 */
bool demo_pdo_encode(const meter_tx_snapshot_t *publication, bool fresh, uint32_t wire,
                     meter_can_frame_t *frame, uint32_t *next_wire);
#endif
