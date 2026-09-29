#include "protocol/can/demo_pdo.h"
#include <string.h>

bool demo_pdo_encode(const meter_tx_snapshot_t *publication, bool fresh, uint32_t wire,
                     meter_can_frame_t *frame, uint32_t *next_wire)
{
    if (!publication || !publication->values || publication->count != DEMO_TX_VALUE_COUNT || !frame ||
        !next_wire || frame->bus != METER_BUS_CAN0 || frame->extended || frame->remote ||
        (frame->id != DEMO_PDO_MOTION_ID && frame->id != DEMO_PDO_STATUS_ID))
        return false;

    /* 整组过期时数值归零；接收者必须先检查 fresh，零仍是合法的新鲜值。 */
    memset(frame->data, 0, sizeof(frame->data));
    frame->size = 8u;
    if (frame->id == DEMO_PDO_MOTION_ID)
    {
        if (fresh)
        {
            uint16_t speed = (uint16_t)publication->values[DEMO_TX_SPEED].value;
            uint16_t height = (uint16_t)publication->values[DEMO_TX_HEIGHT].value;
            uint16_t load = (uint16_t)publication->values[DEMO_TX_LOAD].value;
            frame->data[0] = (uint8_t)speed;
            frame->data[1] = (uint8_t)(speed >> 8);
            frame->data[2] = (uint8_t)height;
            frame->data[3] = (uint8_t)(height >> 8);
            frame->data[4] = (uint8_t)load;
            frame->data[5] = (uint8_t)(load >> 8);
            frame->data[6] = (uint8_t)publication->values[DEMO_TX_SOC].value;
        }
        frame->data[7] = (uint8_t)(((wire & 0x7fu) << 1) | (fresh ? 1u : 0u));
        *next_wire = (wire + 1u) & 0x7fu;
    }
    else
    {
        if (fresh)
        {
            /* 位顺序与独立发送 DBC 一致；不把状态布尔值推断为整车安全结论。 */
            for (unsigned i = 0u; i < 5u; ++i)
                if (publication->values[DEMO_TX_SEAT + i].value != 0)
                    frame->data[0] |= (uint8_t)(1u << i);
        }
        frame->data[1] = fresh ? 1u : 0u;
        frame->data[2] = (uint8_t)wire;
        frame->data[3] = 1u; /* 合成 PDO 布局版本。 */
        frame->data[4] = (uint8_t)publication->generation;
        frame->data[5] = (uint8_t)(publication->generation >> 8);
        frame->data[6] = (uint8_t)(publication->generation >> 16);
        frame->data[7] = (uint8_t)(publication->generation >> 24);
        *next_wire = (wire + 1u) & 0xffu;
    }
    return true;
}
