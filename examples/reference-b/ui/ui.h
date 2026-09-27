#ifndef REFERENCE_B_UI_H
#define REFERENCE_B_UI_H
#include "contracts/meter_product.h"
extern const meter_ui_factory_t product_ui;
/** @brief 返回原生 tabview 当前页面，用于产品级导航验证。 */
unsigned reference_b_page(void *ui);
#endif
