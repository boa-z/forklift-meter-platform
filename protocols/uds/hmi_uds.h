#ifndef HMI_UDS_H
#define HMI_UDS_H
#include "protocols/uds/hmi_uds_did.h"
#include "server.h"
#include "update/hmi_update.h"
/** @brief 语义任务由固定队列按值复制；不携带 UDS 接收缓存指针。 */
typedef enum
{
    HMI_UPDATE_JOB_BEGIN,
    HMI_UPDATE_JOB_WRITE,
    HMI_UPDATE_JOB_VERIFY,
    HMI_UPDATE_JOB_ACTIVATE,
    HMI_UPDATE_JOB_ABORT,
    HMI_UPDATE_JOB_CONFIRM
} hmi_update_job_kind_t;
typedef struct
{
    hmi_update_job_kind_t kind;
    uint32_t generation, offset;
    size_t size;
    hmi_update_manifest_t manifest;
    uint8_t data[HMI_UPDATE_BLOCK_SIZE];
} hmi_update_job_t;
/** @brief 设备端唯一权威诊断快照；CAN 二进制与 UART JSON 都只从这里取数。
 * 字段由端口在锁内复制，UDS 侧不得访问 worker 内部对象。 */
typedef struct
{
    uint8_t state, error;
    bool backend_supported, maintenance, exclusive, admitted;
    bool manifest_valid, candidate_verified;
    uint32_t received, total;
    hmi_update_manifest_t manifest;
    char product[HMI_UDS_ID_SIZE];
    char hardware[HMI_UDS_HARDWARE_SIZE];
    char version[HMI_UDS_SOFTWARE_SIZE];
    char os_file[HMI_UDS_ENDPOINT_OS_FILE];
    char reason[HMI_UDS_ENDPOINT_REASON];
    uint32_t capacity;
} hmi_uds_snapshot_t;
/** @brief Protocol owner 的异步端口；submit 只排队，完成结果保留到领取。 */
typedef struct
{
    void *context;
    bool (*submit)(void *, const hmi_update_job_t *);
    bool (*result)(void *, hmi_update_error_t *, uint32_t *, uint32_t *);
    /** @brief 复制一致性快照；失败时不得返回半份数据。 */
    bool (*snapshot)(void *, hmi_uds_snapshot_t *);
    bool (*can_reset)(void *);
    void (*reset)(void *);
    void (*cancel)(void *);
    /** @brief 可选：编程会话（0x10 02）请求维护模式，离开会话、S3 超时时撤销；Product
     * 的准入钩子仍有最终决定权。 */
    void (*maintenance)(void *, bool);
} hmi_uds_port_t;
/** @brief UDS 协议实例，不拥有 Flash 或 Domain。 */
typedef struct
{
    UDSServer_t server;
    hmi_uds_port_t port;
    /** @brief 0x2E FD10 提交、尚未被 0x34 Begin 接管的临时 Manifest。 */
    hmi_update_manifest_t staged;
    uint32_t generation, offset;
    bool staged_valid, pending;
    hmi_update_job_kind_t pending_kind;
} hmi_uds_t;
/** @brief 初始化固定版本 iso14229 server，由 Protocol thread 周期调用 Poll。
 * 初始化时校验身份字符串长度，超长直接失败而不是截断。 */
bool hmi_uds_init(hmi_uds_t *, UDSTp_t *, const hmi_uds_port_t *);
#endif
