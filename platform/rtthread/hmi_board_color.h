#ifndef HMI_BOARD_COLOR_H
#define HMI_BOARD_COLOR_H
#include "platform/common/hmi_display_ccm.h"
#include <stdbool.h>

/** @brief 显示启动后调用一次：按 AIC_HMI_DISPLAY_GAMMA 校准屏幕色彩、按
 *  AIC_HMI_DISPLAY_CCM 加载开机默认 CCM 表；未配置（gamma 100、CCM 空串）时不动 SDK 默认表。
 *  失败只记日志，不影响显示启动。 */
void hmi_board_color_init(void);
/** @brief 把逐通道 gamma（百分比，100 为 DE 默认）写入显示引擎；返回是否已生效。 */
bool hmi_board_color_apply(unsigned percent);
/** @brief 当前生效的 gamma 百分比。 */
unsigned hmi_board_color_percent(void);
/** @brief 把 CCM 表（含义见 hmi_display_ccm.h）写入显示引擎并置使能；返回是否已生效。 */
bool hmi_board_color_ccm_apply(const int32_t table[HMI_CCM_TAPS]);
/** @brief 读回显示引擎当前生效的 CCM 表与使能位；enabled 可为 NULL。 */
bool hmi_board_color_ccm_read(int32_t table[HMI_CCM_TAPS], bool *enabled);
#endif
