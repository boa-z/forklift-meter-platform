#ifndef METER_UPDATE_PORT_H
#define METER_UPDATE_PORT_H
#include "contracts/meter_update_view.h"
#include "core/meter_core.h"
/** @brief 启动可选 OTA workers；须在 CAN/NVM 初始化之后调用。 */
bool meter_board_update_start(void);
/** @brief App owner 发布准入/健康状态并处理 NVM durable 请求。 */
void meter_board_update_poll(meter_core_t *, bool ui_healthy);
/** @brief 查询维护状态，冻结可持久化设置直到会话结束。 */
bool meter_board_update_maintenance(void);
/** @brief 统一 Protocol owner 投递 UDS 帧；true 表示已由升级协议消费。 */
bool meter_board_update_frame(const meter_can_frame_t *frame);
/** @brief 统一 Protocol owner 驱动 UDS/ISO-TP 超时，不执行 Flash。 */
void meter_board_update_protocol(void);
/** @brief App 请求 worker 协作退出；在途 Flash 返回后退出，不回收活跃资源。 */
void meter_board_update_stop(void);
/** @brief App 查询退出确认；未启动视为已停止。 */
bool meter_board_update_stopped(void);
/** @brief 查询 CAN 接收所有者是否已移交给 Protocol worker。 */
bool meter_board_update_started(void);
/** @brief 查询 Product 独占维护模式；该模式暂停正常业务。 */
bool meter_board_update_exclusive(void);
/** @brief 在锁内复制只读展示值，由 App 交给 UI；不暴露 worker 内存。 */
void meter_board_update_view(meter_update_view_t *view);
#endif
