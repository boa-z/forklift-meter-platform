#ifndef METER_UPDATE_WIDGET_H
#define METER_UPDATE_WIDGET_H
#include "contracts/meter_update_view.h"
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <lvgl.h>
/** @brief Product 提供本地化文案；控件不持有文案指针。 */
typedef struct
{
    const char *title, *phase, *note, *current, *target, *error, *preview;
} meter_update_widget_text_t;
/** @brief 调用方持有控件存储；root 及子对象随 LVGL 父对象删除。 */
typedef struct
{
    lv_obj_t *root, *title, *phase, *bar, *percent, *versions, *error, *note, *preview;
} meter_update_widget_t;
/** @brief 创建默认隐藏的全屏控件；只允许 UI owner 调用一次。 */
bool meter_update_widget_create(meter_update_widget_t *widget, lv_obj_t *parent);
/** @brief 展示值复制快照；百分比只表示传输，阶段独立展示，不执行业务操作。 */
void meter_update_widget_present(meter_update_widget_t *widget, const meter_update_view_t *view,
                                 const meter_update_widget_text_t *text, meter_language_t language);
#endif
