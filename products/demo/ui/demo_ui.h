#ifndef DEMO_UI_H
#define DEMO_UI_H
#include "contracts/meter_product.h"
void *demo_ui_create(void *parent, const meter_ui_actions_t *actions);
void demo_ui_present(void *ui, const meter_snapshot_t *snapshot, uint32_t elapsed_ms);
void demo_ui_destroy(void *ui);
unsigned demo_ui_active_page(const void *ui);
/** @brief 展示只读升级状态；preview 为 Host 演示标记，不操作设备。 */
void demo_ui_update(void *ui, const meter_update_view_t *view, meter_language_t language);
void demo_ui_update_preview(void *ui, const meter_update_view_t *view, meter_language_t language);
#endif
