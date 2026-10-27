#ifndef PRODUCT_UI_H
#define PRODUCT_UI_H
#include <stdbool.h>
/* 主机 UI 工厂和固件语言钩子共用；必须在 LVGL 与翻译服务初始化之后调用。 */
bool product_locale_init(void);
#endif
