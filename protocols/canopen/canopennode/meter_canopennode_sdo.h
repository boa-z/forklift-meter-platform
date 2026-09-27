#ifndef METER_CANOPENNODE_SDO_H
#define METER_CANOPENNODE_SDO_H
#include "301/CO_SDOclient.h"
#include "diagnostics/meter_diagnostics.h"
#include "meter_canopennode_driver.h"
/** @brief 单通道固定容量，终态须取走才能复用槽位；对象初始化后不得按值复制。 */
#define METER_SDO_CAPACITY 4u
#define METER_SDO_PAYLOAD_SIZE 128u
/** @brief 产品可见的读写操作，不暴露线上传输方向术语。 */
typedef enum
{
    METER_SDO_READ,
    METER_SDO_WRITE
} meter_sdo_operation_t;
/** @brief 请求生命周期；IDLE 表示空槽，其余终态保留到调用者取走。 */
typedef enum
{
    METER_SDO_IDLE,
    METER_SDO_PENDING,
    METER_SDO_SUCCESS,
    METER_SDO_ABORTED,
    METER_SDO_TIMEOUT
} meter_sdo_status_t;
/** @brief 读写请求；size 为读容量或写长度，retry_count 为额外尝试次数，abort 默认不重试。 */
typedef struct
{
    uint32_t request_id;
    uint8_t node_id;
    uint16_t index;
    uint8_t subindex;
    meter_sdo_operation_t operation;
    uint8_t payload[METER_SDO_PAYLOAD_SIZE];
    size_t size;
    uint16_t timeout_ms, retry_delay_ms;
    uint8_t retry_count;
    bool retry_abort;
} meter_sdo_request_t;
/** @brief 生命周期结果与上游 abort code；payload 仅在 SUCCESS 时有效。 */
typedef struct
{
    uint32_t request_id;
    meter_sdo_status_t status;
    uint32_t abort_code;
    uint16_t attempts;
    size_t size;
    uint8_t payload[METER_SDO_PAYLOAD_SIZE];
} meter_sdo_result_t;
/** @brief 静态分配的 SDO 通道；内部字段由实现维护。 */
typedef struct
{
    CO_SDOclient_t client;
    meter_diagnostics_t *diag;
    CO_CANmodule_t can;
    CO_CANrx_t rx;
    CO_CANtx_t tx;
    meter_sdo_request_t requests[METER_SDO_CAPACITY];
    meter_sdo_result_t results[METER_SDO_CAPACITY];
    uint8_t queue[METER_SDO_CAPACITY], head, count;
    int active;
    uint32_t last_ms, retry_at;
    bool initialized, clock_set, retry_wait, overflow;
} meter_sdo_channel_t;
/** @brief 初始化静态通道；不创建 NMT、Heartbeat 或车辆 OD。 */
bool meter_sdo_init(meter_sdo_channel_t *channel, meter_bus_role_t bus);
/** @brief 排队一条读写请求并复制载荷；容量满或 ID 重复时返回 false。 */
bool meter_sdo_submit(meter_sdo_channel_t *channel, const meter_sdo_request_t *request);
/** @brief 串行推进上游协议计时与业务重试；tx 端口只在此调用期间使用。 */
void meter_sdo_process(meter_sdo_channel_t *channel, uint32_t now_ms, const meter_can_tx_port_t *tx);
/** @brief 将响应投递上游；空闲、重试等待与未发送状态下拒绝迟到帧。 */
bool meter_sdo_receive(meter_sdo_channel_t *channel, const meter_can_frame_t *frame);
/** @brief 查询请求状态，不消耗结果。 */
bool meter_sdo_result(const meter_sdo_channel_t *channel, uint32_t request_id, meter_sdo_result_t *result);
/** @brief 取走终态并释放容量，PENDING 不允许取走。 */
bool meter_sdo_take(meter_sdo_channel_t *channel, uint32_t request_id, meter_sdo_result_t *result);
/** @brief 断连或复位时取消在途与排队请求，终态保留供业务读取。 */
void meter_sdo_reset(meter_sdo_channel_t *channel);
/** @brief 绑定仅含 0x1280 的静态客户端参数对象。 */
CO_ReturnError_t meter_co_sdo_init_od(CO_SDOclient_t *client, CO_CANmodule_t *can);
/** @brief 绑定可选诊断实例，调用者保证与通道同线程并覆盖通道生命周期。 */
void meter_sdo_bind_diagnostics(meter_sdo_channel_t *channel, meter_diagnostics_t *diag);
#endif
