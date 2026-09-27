#ifndef METER_CO_DRIVER_TARGET_H
#define METER_CO_DRIVER_TARGET_H
#include "contracts/meter_protocol.h"
#include <stddef.h>
#include <stdint.h>

/** @file CO_driver_target.h
 * @brief 静态 SDO 模块配置；所有入口由同一协议线程串行调用，ISR 仅投递原生 IPC。
 */
#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE | CO_CONFIG_SDO_CLI_SEGMENTED)
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 128
#define CO_CONFIG_FIFO CO_CONFIG_FIFO_ENABLE
#define CO_CONFIG_SDO_SRV CO_CONFIG_SDO_SRV_SEGMENTED
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 128
#define CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC 0
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "当前桥接仅支持小端目标"
#endif
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) (x)
#define CO_SWAP_32(x) (x)
#define CO_SWAP_64(x) (x)
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;
#define CO_CANrxMsg_readIdent(msg) ((uint16_t)((const meter_can_frame_t *)(msg))->id)
#define CO_CANrxMsg_readDLC(msg) (((const meter_can_frame_t *)(msg))->size)
#define CO_CANrxMsg_readData(msg) (((const meter_can_frame_t *)(msg))->data)
typedef struct
{
    uint16_t ident, mask;
    void *object;
    void (*CANrx_callback)(void *, void *);
} CO_CANrx_t;
typedef struct
{
    uint32_t ident;
    uint8_t DLC, data[8];
    volatile bool_t bufferFull, syncFlag;
} CO_CANtx_t;
typedef struct
{
    struct meter_diagnostics *diag;
    CO_CANrx_t *rxArray;
    CO_CANtx_t *txArray;
    uint16_t rxSize, txSize, CANerrorStatus;
    bool_t CANnormal;
    meter_bus_role_t bus;
    meter_can_tx_port_t port;
    uint32_t now_ms;
} CO_CANmodule_t;
/* 协议线程独占本模块，跨线程消息由宿主 IPC 排队，禁止直接在 ISR 调用。 */
#define CO_LOCK_CAN_SEND(module) ((void)(module))
#define CO_UNLOCK_CAN_SEND(module) ((void)(module))
#define CO_LOCK_OD(module) ((void)(module))
#define CO_UNLOCK_OD(module) ((void)(module))
#define CO_FLAG_READ(flag) ((flag) != NULL)
#define CO_FLAG_SET(flag) ((flag) = (void *)1)
#define CO_FLAG_CLEAR(flag) ((flag) = NULL)
#endif
