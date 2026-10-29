#ifndef HMI_UPDATE_H
#define HMI_UPDATE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define HMI_UPDATE_BLOCK_SIZE 512u
#define HMI_UPDATE_ID_SIZE 32u
#include "contracts/hmi_can_frame.h"
#include "contracts/hmi_update_view.h"
/** @brief 可诊断错误；激活写入错误不能解释为已回滚。 */
typedef enum
{
    HMI_UPDATE_OK,
    HMI_UPDATE_STATE,
    HMI_UPDATE_COMPATIBILITY,
    HMI_UPDATE_DENIED,
    HMI_UPDATE_LENGTH,
    HMI_UPDATE_ORDER,
    HMI_UPDATE_BACKEND,
    HMI_UPDATE_HASH,
    HMI_UPDATE_TIMEOUT,
    HMI_UPDATE_NVM,
    HMI_UPDATE_SESSION,
    HMI_UPDATE_FORMAT,
    HMI_UPDATE_UNSUPPORTED
} hmi_update_error_t;
/** @brief 小型包描述；完整 SHA256 仅用于完整性，不提供身份认证。 */
typedef struct
{
    char product[HMI_UPDATE_ID_SIZE], hardware[HMI_UPDATE_ID_SIZE];
    char version[HMI_UPDATE_VERSION_SIZE];
    uint32_t size;
    uint8_t sha256[32];
} hmi_update_manifest_t;
/** @brief Product 明确选择升级身份、容量、超时与 UDS 寻址。 */
typedef struct hmi_update_policy
{
    const char *product, *hardware;
    uint32_t max_size, timeout_ms;
    hmi_bus_role_t bus;              /* UDS 所在逻辑总线，零值为 CAN0 */
    uint32_t request_id, response_id;  /* 11 位物理寻址；零值使用 0x7E0/0x7E8 */
} hmi_update_policy_t;
/** @brief 只有独立 Update worker 调用此同步后端，禁止在协议回调中调用。 */
typedef struct
{
    void *context;
    hmi_update_error_t (*begin)(void *, const hmi_update_manifest_t *);
    hmi_update_error_t (*write)(void *, const uint8_t *, size_t);
    hmi_update_error_t (*verify)(void *, const hmi_update_manifest_t *);
    hmi_update_error_t (*activate)(void *, const hmi_update_manifest_t *);
    void (*abort)(void *);
} hmi_update_backend_t;
/** @brief worker 单一所有者；跨线程只能复制状态，不共享可变实例。 */
typedef struct
{
    hmi_update_policy_t policy;
    hmi_update_backend_t backend;
    hmi_update_manifest_t manifest;
    hmi_update_state_t state;
    hmi_update_error_t error;
    uint32_t generation, received, last_activity, started_ms, completed_ms;
    bool opened;
} hmi_firmware_update_t;
/** @brief 初始化有界服务，不访问 Flash。 */
bool hmi_update_init(hmi_firmware_update_t *, const hmi_update_policy_t *,
                       const hmi_update_backend_t *);
/** @brief 开始新会话；准入必须来自 Product/App，不能信任上位机声明。 */
hmi_update_error_t hmi_update_begin(hmi_firmware_update_t *, const hmi_update_manifest_t *,
                                        bool admitted, uint32_t now);
/** @brief 写入唯一下一块；generation/offset 隔离旧会话和乱序。 */
hmi_update_error_t hmi_update_write(hmi_firmware_update_t *, uint32_t generation, uint32_t offset,
                                        const uint8_t *, size_t, uint32_t now);
/** @brief 完整接收后校验包与候选；未验证不得激活。 */
hmi_update_error_t hmi_update_verify(hmi_firmware_update_t *, uint32_t generation, uint32_t now);
/** @brief 激活要求本地 NVM barrier 已满足；失败保留明确错误。 */
hmi_update_error_t hmi_update_activate(hmi_firmware_update_t *, uint32_t generation, bool durable);
/** @brief 终止下载/候选；激活成功后不能用 Abort 伪装取消。 */
hmi_update_error_t hmi_update_abort(hmi_firmware_update_t *, uint32_t generation);
/** @brief 检测下载超时并释放后端；无隐式重启。 */
void hmi_update_tick(hmi_firmware_update_t *, uint32_t now);
/** @brief 查询静态状态名称。 */
const char *hmi_update_state_name(hmi_update_state_t);
#endif
