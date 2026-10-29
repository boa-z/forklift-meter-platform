#ifndef HMI_DOMAIN_FIXTURE_H
#define HMI_DOMAIN_FIXTURE_H
#include "core/hmi_core.h"
/** @brief Host 读取预验证的更新流；仅经 core 写入，禁止直接操作 UI。 */
bool hmi_fixture_load(hmi_core_t *core, const char *path);
#endif
