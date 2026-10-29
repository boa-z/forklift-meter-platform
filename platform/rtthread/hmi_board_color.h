#ifndef HMI_BOARD_COLOR_H
#define HMI_BOARD_COLOR_H

/** @brief 显示启动后调用一次：按 AIC_HMI_DISPLAY_GAMMA 校准屏幕色彩、按
 *  AIC_HMI_DISPLAY_CCM 加载开机默认 CCM 表；未配置（gamma 100、CCM 空串）时不动 SDK 默认表。
 *  失败只记日志，不影响显示启动。实时调色是台架工具（Luban-Lite Display Lab），不在 Framework 里。 */
void hmi_board_color_init(void);
#endif
