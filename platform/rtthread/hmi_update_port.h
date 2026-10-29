#ifndef HMI_UPDATE_PORT_H
#define HMI_UPDATE_PORT_H
#include "contracts/hmi_update_view.h"
#include "core/hmi_core.h"
/** @brief 运行时线程启动 OTA worker；须在 CAN/NVM 初始化之后调用。以下函数除说明外都只由运行时线程调用。 */
bool hmi_board_update_start(void);
/** @brief 发布准入/健康状态并处理 NVM durable 请求。 */
void hmi_board_update_poll(hmi_core_t *, bool ui_healthy);
/** @brief 查询维护状态，冻结可持久化设置直到会话结束。 */
bool hmi_board_update_maintenance(void);
/** @brief 投递 UDS 帧；true 表示已由升级协议消费。 */
bool hmi_board_update_frame(const hmi_can_frame_t *frame);
/** @brief 驱动 UDS/ISO-TP 超时，不执行 Flash。 */
void hmi_board_update_protocol(void);
/** @brief 查询 CAN 接收所有者是否已移交给 Protocol worker。 */
bool hmi_board_update_started(void);
/** @brief 查询 Product 独占维护模式；该模式暂停正常业务。 */
bool hmi_board_update_exclusive(void);
/** @brief 在锁内复制只读展示值，由 App 交给 UI；不暴露 worker 内存。 */
void hmi_board_update_view(hmi_update_view_t *view);
#endif
