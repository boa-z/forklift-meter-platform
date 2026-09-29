#ifndef DEMO_SETTINGS_APP_H
#define DEMO_SETTINGS_APP_H
#include "contracts/meter_product.h"
/* 只有 App owner 调用；UI 仅发送 presentation.h 定义的语义意图。 */
bool demo_settings_action(meter_snapshot_t *snapshot, const meter_action_t *action, uint32_t now_ms);
void demo_settings_reset(uint32_t generation);
void demo_settings_run(uint32_t now_ms, const meter_command_port_t *commands);
void demo_settings_publish(meter_snapshot_t *snapshot);
#endif
