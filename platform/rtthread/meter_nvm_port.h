#ifndef METER_NVM_PORT_H
#define METER_NVM_PORT_H
#include "core/meter_settings.h"
#include "diagnostics/meter_diagnostics.h"
#include "storage/meter_nvm.h"
/* 除 worker 线程外，下列函数都只由运行时线程调用；介质 I/O 全在 worker 里完成。 */
/** @brief 按 Product 存储策略启动 NVM worker 并投递首次读取；调用前 Core 已初始化。 */
bool meter_board_nvm_start(meter_core_t *core);
/** @brief 领取 worker 结果、处理 MSH 维护请求并调度保存；不做阻塞 I/O。 */
void meter_board_nvm_poll(uint32_t now, meter_diag_storage_t *status);
/** @brief 设置可能变化后调用：编码当前设置，内容不同才进入去抖保存。 */
bool meter_board_nvm_changed(uint32_t now);
/** @brief 启动读取是否已结束（未启用持久化视为已结束），避免迟到的读取覆盖用户修改。 */
bool meter_board_nvm_ready(void);
/** @brief 升级激活前申请立即保存，返回需要等到 durable 的版本号；0 表示无需等待。 */
uint64_t meter_board_nvm_flush(void);
/** @brief 目标版本是否已经写入介质。 */
bool meter_board_nvm_barrier(uint64_t target);
#endif
