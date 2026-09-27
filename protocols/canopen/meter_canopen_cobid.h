#ifndef METER_CANOPEN_COBID_H
#define METER_CANOPEN_COBID_H
#include <stdint.h>
/** @file meter_canopen_cobid.h
 * @brief 标准预定义连接集 COB-ID 基础计算，不包含协议编解码。
 */
#define METER_CANOPEN_COB_TPDO1_BASE 0x180u
#define METER_CANOPEN_COB_TPDO2_BASE 0x280u
#define METER_CANOPEN_COB_RPDO1_BASE 0x200u
#define METER_CANOPEN_COB_RPDO2_BASE 0x300u
#define METER_CANOPEN_COB_SDO_REQ_BASE 0x600u
#define METER_CANOPEN_COB_SDO_RESP_BASE 0x580u

/** @brief 由预定义连接集基址与有效节点号计算 COB-ID。 */
static inline uint32_t meter_canopen_cob(uint32_t base, uint8_t node_id)
{
    return base + (uint32_t)node_id;
}

#endif
