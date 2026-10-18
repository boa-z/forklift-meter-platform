#ifndef METER_UDS_H
#define METER_UDS_H
#include "protocols/uds/meter_uds_did.h"
#include "server.h"
#include "update/meter_update.h"
/** @brief 语义任务由固定队列按值复制；不携带 UDS 接收缓存指针。 */
typedef enum
{
    METER_UPDATE_JOB_BEGIN,
    METER_UPDATE_JOB_WRITE,
    METER_UPDATE_JOB_VERIFY,
    METER_UPDATE_JOB_ACTIVATE,
    METER_UPDATE_JOB_ABORT,
    METER_UPDATE_JOB_CONFIRM
} meter_update_job_kind_t;
typedef struct
{
    meter_update_job_kind_t kind;
    uint32_t generation, offset;
    size_t size;
    meter_update_manifest_t manifest;
    uint8_t data[METER_UPDATE_BLOCK_SIZE];
} meter_update_job_t;
/** @brief 设备端唯一权威诊断快照；CAN 二进制与 UART JSON 都只从这里取数。
 * 字段由端口在锁内复制，UDS 侧不得访问 worker 内部对象。 */
typedef struct
{
    uint8_t state, error;
    bool backend_supported, maintenance, exclusive, admitted;
    bool manifest_valid, candidate_verified;
    uint32_t received, total;
    meter_update_manifest_t manifest;
    char product[METER_UDS_ID_SIZE];
    char hardware[METER_UDS_HARDWARE_SIZE];
    char version[METER_UDS_SOFTWARE_SIZE];
    char os_file[METER_UDS_ENDPOINT_OS_FILE];
    char reason[METER_UDS_ENDPOINT_REASON];
    uint32_t capacity;
} meter_uds_snapshot_t;
/** @brief Protocol owner 的异步端口；submit 只排队，完成结果保留到领取。 */
typedef struct
{
    void *context;
    bool (*submit)(void *, const meter_update_job_t *);
    bool (*result)(void *, meter_update_error_t *, uint32_t *, uint32_t *);
    /** @brief 复制一致性快照；失败时不得返回半份数据。 */
    bool (*snapshot)(void *, meter_uds_snapshot_t *);
    bool (*can_reset)(void *);
    void (*reset)(void *);
    void (*cancel)(void *);
    /** @brief 可选：编程会话（0x10 02）请求维护模式，离开会话、S3 超时时撤销；Product
     * 的准入钩子仍有最终决定权。 */
    void (*maintenance)(void *, bool);
} meter_uds_port_t;
/** @brief UDS 协议实例，不拥有 Flash 或 Domain。 */
typedef struct
{
    UDSServer_t server;
    meter_uds_port_t port;
    /** @brief 0x2E FD10 提交、尚未被 0x34 Begin 接管的临时 Manifest。 */
    meter_update_manifest_t staged;
    uint32_t generation, offset;
    bool staged_valid, pending;
    meter_update_job_kind_t pending_kind;
} meter_uds_t;
/** @brief 初始化固定版本 iso14229 server，由 Protocol thread 周期调用 Poll。
 * 初始化时校验身份字符串长度，超长直接失败而不是截断。 */
bool meter_uds_init(meter_uds_t *, UDSTp_t *, const meter_uds_port_t *);
#endif
