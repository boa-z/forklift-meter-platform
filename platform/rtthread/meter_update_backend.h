#ifndef METER_UPDATE_BACKEND_H
#define METER_UPDATE_BACKEND_H
#include "update/meter_update.h"
/* 升级包里的 OS 镜像名随芯片变化（<chip>_os.itb）；SConscript 按 PRJ_CHIP 定义 METER_UPDATE_CHIP。 */
#ifndef METER_UPDATE_CHIP
#define METER_UPDATE_CHIP d13x
#endif
#define METER_UPDATE_TEXT_(x) #x
#define METER_UPDATE_TEXT(x) METER_UPDATE_TEXT_(x)
#define METER_UPDATE_OS_FILE METER_UPDATE_TEXT(METER_UPDATE_CHIP) "_os.itb"
/** @brief 返回原生 OS-only 安装器；SDK 全局状态只能由单一 Update worker 拥有。 */
meter_update_backend_t meter_aic_update_backend(void);
/** @brief 查询当前 SDK 是否满足应用要求的完整升级契约。 */
bool meter_aic_update_supported(void);
/** @brief 返回稳定的能力阻塞原因，不将自动确认当作健康确认。 */
const char *meter_aic_update_reason(void);
/** @brief 当前 SDK 不能提供应用健康门禁后的显式确认，返回失败。 */
int meter_aic_update_confirm(void);
#ifdef METER_AIC_OTA
/** @brief 启动时只读审计本机 ENV 与候选 NAND 几何，不执行擦写。 */
bool meter_aic_update_prepare(void);
/** @brief 返回经本机审计的非活动 OS 分区容量。 */
uint32_t meter_aic_update_capacity(void);
#endif
#endif
