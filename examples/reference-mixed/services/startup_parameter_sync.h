#ifndef MIXED_STARTUP_PARAMETER_SYNC_H
#define MIXED_STARTUP_PARAMETER_SYNC_H
#include "protocols/canopen/canopennode/meter_canopennode_sdo.h"
/** @brief 产品启动参数同步状态，只有成功读取两项后才进入 READY。 */
typedef enum
{
    MIXED_SYNC_WAIT_DATA,
    MIXED_SYNC_READ_A,
    MIXED_SYNC_READ_B,
    MIXED_SYNC_READY,
    MIXED_SYNC_FAILED
} mixed_sync_phase_t;
/** @brief 客户业务状态，不保存或解析 CAN 帧。 */
typedef struct
{
    mixed_sync_phase_t phase;
    uint16_t parameters[2];
    uint32_t error;
} mixed_startup_sync_t;
/** @brief 复位产品工作流，等待下一份有效 PDO。 */
void mixed_startup_reset(mixed_startup_sync_t *service);
/** @brief 通过语义化读请求串行同步；返回本轮是否刚进入 READY。 */
bool mixed_startup_process(mixed_startup_sync_t *service, meter_sdo_channel_t *sdo, bool data_ready);
#endif
