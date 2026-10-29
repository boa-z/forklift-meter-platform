#ifndef HMI_UPDATE_VIEW_H
#define HMI_UPDATE_VIEW_H
#include <stdbool.h>
#include <stdint.h>
#define HMI_UPDATE_VERSION_SIZE 48u
/** @brief 升级阶段；接收完成不等于候选验证或激活成功。 */
typedef enum
{
    HMI_UPDATE_IDLE,
    HMI_UPDATE_DOWNLOADING,
    HMI_UPDATE_TRANSFERRED,
    HMI_UPDATE_VERIFYING,
    HMI_UPDATE_CANDIDATE,
    HMI_UPDATE_WAIT_DURABLE,
    HMI_UPDATE_ACTIVATING,
    HMI_UPDATE_ACTIVATED,
    HMI_UPDATE_CONFIRMED,
    HMI_UPDATE_ABORTED,
    HMI_UPDATE_FAILED
} hmi_update_state_t;
/** @brief App 复制的只读升级视图；不包含协议、存储句柄或操作回调。 */
typedef struct
{
    bool visible;
    hmi_update_state_t state;
    uint32_t received, total, error;
    char current_version[HMI_UPDATE_VERSION_SIZE], target_version[HMI_UPDATE_VERSION_SIZE];
} hmi_update_view_t;
#endif
