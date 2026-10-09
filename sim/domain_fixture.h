#ifndef METER_DOMAIN_FIXTURE_H
#define METER_DOMAIN_FIXTURE_H
#include "core/meter_core.h"
/** @brief Host 读取预验证的更新流；仅经 core 写入，禁止直接操作 UI。 */
bool meter_fixture_load(meter_core_t *core, const char *path);
#endif
