#define OD_DEFINITION
#include "meter_canopennode_sdo.h"

CO_ReturnError_t meter_co_sdo_init_od(CO_SDOclient_t *client, CO_CANmodule_t *can)
{
    /* 上游初始化只读这些参数；后续每次请求通过 setup 选择远端节点。 */
    static uint8_t count = 3, node = 1;
    static uint32_t request_cob = 0x601, response_cob = 0x581;
    static OD_obj_record_t record[] = {{&count, 0, ODA_SDO_R, 1},
                                       {&request_cob, 1, ODA_SDO_R, 4},
                                       {&response_cob, 2, ODA_SDO_R, 4},
                                       {&node, 3, ODA_SDO_R, 1}};
    static OD_entry_t entry = {0x1280, 4, ODT_REC, record, NULL};
    return CO_SDOclient_init(client, NULL, &entry, 0, can, 0, can, 0, NULL);
}
