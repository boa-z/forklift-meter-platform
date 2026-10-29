#ifndef HMI_UPDATE_BACKEND_H
#define HMI_UPDATE_BACKEND_H
#include "update/hmi_update.h"
/* 升级包里的 OS 镜像名随芯片变化（<chip>_os.itb）；SConscript 按 PRJ_CHIP 定义 HMI_UPDATE_CHIP。 */
#ifndef HMI_UPDATE_CHIP
#define HMI_UPDATE_CHIP d13x
#endif
#define HMI_UPDATE_TEXT_(x) #x
#define HMI_UPDATE_TEXT(x) HMI_UPDATE_TEXT_(x)
#define HMI_UPDATE_OS_FILE HMI_UPDATE_TEXT(HMI_UPDATE_CHIP) "_os.itb"
/** @brief 返回原生 OS-only 安装器；SDK 全局状态只能由单一 Update worker 拥有。 */
hmi_update_backend_t hmi_aic_update_backend(void);
/** @brief 查询当前 SDK 是否满足应用要求的完整升级契约。 */
bool hmi_aic_update_supported(void);
/** @brief 返回稳定的能力阻塞原因，不将自动确认当作健康确认。 */
const char *hmi_aic_update_reason(void);
/** @brief 当前 SDK 不能提供应用健康门禁后的显式确认，返回失败。 */
int hmi_aic_update_confirm(void);
#ifdef HMI_AIC_OTA
/** @brief 启动时只读审计本机 ENV 与候选 NAND 几何，不执行擦写。 */
bool hmi_aic_update_prepare(void);
/** @brief 返回经本机审计的非活动 OS 分区容量。 */
uint32_t hmi_aic_update_capacity(void);
#endif
#endif
