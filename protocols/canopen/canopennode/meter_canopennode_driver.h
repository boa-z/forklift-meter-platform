#ifndef METER_CANOPENNODE_DRIVER_H
#define METER_CANOPENNODE_DRIVER_H
#include "301/CO_driver.h"
/** @brief 将标准数据帧交给匹配的上游接收回调；须在协议线程调用。 */
bool meter_co_receive(CO_CANmodule_t *module, const meter_can_frame_t *frame);
/** @brief 重试未被传输端口接受的发送缓冲；成功后才清除 bufferFull。 */
bool meter_co_flush(CO_CANmodule_t *module);
#endif
