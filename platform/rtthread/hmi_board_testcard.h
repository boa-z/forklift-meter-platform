#ifndef HMI_BOARD_TESTCARD_H
#define HMI_BOARD_TESTCARD_H

/** @brief 显示启动后调用一次（UI 线程）：挂上色卡请求轮询定时器，之后 hmi_testcard 可用。
 *  失败只记日志，不影响显示启动。 */
void hmi_board_testcard_init(void);
#endif
