#ifndef METER_CAN_FRAME_H
#define METER_CAN_FRAME_H
#include <stdbool.h>
#include <stdint.h>
typedef enum
{
    METER_BUS_CAN0,
    METER_BUS_CAN1,
    METER_BUS_COUNT
} meter_bus_role_t;
typedef struct
{
    meter_bus_role_t bus;
    uint32_t id;
    uint32_t timestamp_ms;
    bool extended;
    bool remote;
    uint8_t size;
    uint8_t data[8];
} meter_can_frame_t;
typedef uint16_t meter_frame_route_owner_t;
/* 一条总线/帧格式的后备路由，用于原始报文监视；精确 ID 路由优先。 */
#define METER_CAN_ID_ANY UINT32_MAX
typedef struct
{
    meter_bus_role_t bus;
    uint32_t id;
    bool extended;
    meter_frame_route_owner_t owner;
} meter_frame_route_t;
#endif
