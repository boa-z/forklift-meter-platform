#ifndef METER_TEST_SDO_PEER_H
#define METER_TEST_SDO_PEER_H
#ifndef OD_DEFINITION
#define OD_DEFINITION
#endif
#include "301/CO_SDOserver.h"
#include "protocols/canopen/canopennode/meter_canopennode_sdo.h"
/** @brief 仅宿主测试使用的上游 SDO Server 与合成 OD。 */
typedef struct
{
    CO_SDOserver_t server;
    CO_CANmodule_t can;
    CO_CANrx_t rx;
    CO_CANtx_t tx;
    OD_t od;
    OD_entry_t entries[6];
    OD_obj_var_t vars[5];
    OD_obj_record_t params[3];
    uint8_t param_count;
    uint16_t parameter_a, parameter_b;
    uint8_t data[5][128];
    meter_can_frame_t incoming[256], outgoing[256], sent[1024];
    unsigned in_count, out_count, sent_count;
    unsigned busy;
    bool drop;
} sdo_peer_t;
/** @brief 初始化合成节点；不创建或发送 NMT、Heartbeat。 */
bool sdo_peer_init(sdo_peer_t *peer);
/** @brief 客户端 TX 端口，模拟忙与丢包。 */
bool sdo_peer_send(void *context, const meter_can_frame_t *frame);
/** @brief 推进真实上游服务端，响应暂存供调用方投递。 */
void sdo_peer_step(sdo_peer_t *peer);
#endif
