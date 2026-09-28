#ifndef MIXED_STARTUP_PARAMETER_SYNC_H
#define MIXED_STARTUP_PARAMETER_SYNC_H
#include "contracts/meter_protocol.h"
/** @brief App 独占的产品启动同步状态。 */
typedef enum { MIXED_SYNC_WAIT_DATA, MIXED_SYNC_READ_A, MIXED_SYNC_READ_B, MIXED_SYNC_READY, MIXED_SYNC_FAILED } mixed_sync_phase_t;
/** @brief 只保存语义参数与请求身份，禁止访问协议通道。 */
typedef struct {
    mixed_sync_phase_t phase;
    uint16_t parameters[2];
    uint32_t error, deadline, generation;
    bool data_ready, received[2];
    meter_request_id_t request;
} mixed_startup_sync_t;
extern mixed_startup_sync_t mixed_startup;
/** @brief App generation 切换时调用，旧协议结果由端口隔离。 */
void mixed_startup_reset(uint32_t generation);
/** @brief App 消费已经解码的参数、freshness 和失败事件。 */
void mixed_startup_event(const meter_protocol_event_t *event);
/** @brief App runnable；有界且不阻塞，由命令端口驱动两项串行读取。 */
void mixed_startup_run(uint32_t now, const meter_command_port_t *port);
#endif
