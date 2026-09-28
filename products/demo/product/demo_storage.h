#ifndef DEMO_STORAGE_H
#define DEMO_STORAGE_H
#include "core/meter_settings.h"
#include "generated/demo_catalog.h"
/** @brief Demo 产品的全部域内存。
 *
 * 槽位数与目录条目同源，扩大产品只需改 schema，永远不需要改动平台。整块为静态存储，不含堆。
 * settings 尾部紧跟参数区，长度由 meter_settings_size() 决定。
 */
typedef struct
{
    meter_value_t signals[DEMO_SIGNAL_SLOTS];
    float parameters[DEMO_PARAMETER_SLOTS];
    meter_fault_state_t faults[DEMO_FAULT_SLOTS];
    uint8_t settings[METER_SETTINGS_OVERHEAD + DEMO_PARAMETER_SLOTS * METER_SETTINGS_ENTRY_SIZE];
} demo_domain_store_t;
/** @brief 把产品存储绑定为 core 需要的形式。
 *
 * 只填指针与容量，不初始化内容。store 必须比引用它的 core 活得更久。
 */
meter_core_storage_t demo_domain_bind(demo_domain_store_t *store);
#endif
