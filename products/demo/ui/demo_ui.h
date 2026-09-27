#ifndef DEMO_UI_H
#define DEMO_UI_H
#include "contracts/meter_product.h"
void *demo_ui_create(void *parent, const meter_ui_actions_t *actions);
void demo_ui_present(void *ui, const meter_snapshot_t *snapshot, uint32_t elapsed_ms);
void demo_ui_destroy(void *ui);
unsigned demo_ui_active_page(const void *ui);
#endif
