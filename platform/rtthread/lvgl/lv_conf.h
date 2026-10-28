/* SPDX-License-Identifier: Apache-2.0 */
/* 应用侧的 LVGL 配置：先取 SDK 自己的 lv_conf.h，再叠加所选 Product 的原生选项，SDK 源码不改。
   SConscript 把本目录放在 include 路径最前面，使 LVGL 的 "lv_conf.h" 先找到这份；Product 头文件的路径
   由构建时生成的 meter_product_lv_conf.h 给出。Product 要改 SDK 已经定义的选项时先 #undef 再定义。 */
#ifndef METER_LV_CONF_H
#define METER_LV_CONF_H
#include_next "lv_conf.h"
#include "meter_product_lv_conf.h"
#endif /* METER_LV_CONF_H */
