#ifndef MIXED_CANOPEN_H
#define MIXED_CANOPEN_H
#include "contracts/meter_product.h"
#include "protocols/canopen/meter_canopen_profile.h"
#include "protocols/canopen/canopennode/meter_canopennode_sdo.h"
#include "runtime/meter_execution.h"
#include "canopen/mixed_commands.h"
#define MIXED_CANOPEN_NODE_ID 12u
#define MIXED_CANOPEN_SOURCE 32u
#define MIXED_CANOPEN_PDO_TIMEOUT_MS 500u
#define MIXED_CANOPEN_SDO_TIMEOUT_MS 120u
#define MIXED_CANOPEN_SDO_RETRIES 3u
#define MIXED_CANOPEN_TPDO_PERIOD_MS 200u
extern const meter_canopen_profile_t mixed_canopen_profile;
/** @brief 静态产品协议状态；含自引用通道，初始化后禁止复制。 */
typedef struct
{
    uint8_t node_id;
    uint32_t rpdo1_cob, tpdo1_cob, sdo_resp_cob;
    uint32_t last_rpdo_ms, last_tpdo_ms;
    bool rpdo_seen, timeout_reported, tpdo_armed;
    meter_sdo_channel_t sdo;
    meter_deadline_t tpdo_deadline;
    uint16_t command_kinds[METER_SDO_CAPACITY];
    uint32_t next_request_id, commands[METER_SDO_CAPACITY];
    meter_request_id_t command_requests[METER_SDO_CAPACITY];
    unsigned pdo_frames, timeouts;
} mixed_canopen_state_t;
/** @brief 初始化固定 PDO 绑定与独立 SDO 通道。 */
void mixed_canopen_init(mixed_canopen_state_t *state);
extern const meter_protocol_adapter_t mixed_canopen_adapter;
extern mixed_canopen_state_t mixed_canopen_state;
/** @brief Product 根据命令选择路由属主，Application 不知道协议属主。 */
bool mixed_command_route(void *context, const meter_command_t *command, meter_frame_route_owner_t *owner_out);
#endif
