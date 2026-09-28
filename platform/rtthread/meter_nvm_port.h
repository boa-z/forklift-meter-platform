#ifndef METER_NVM_PORT_H
#define METER_NVM_PORT_H
#include "core/meter_settings.h"
#include "diagnostics/meter_diagnostics.h"
#include "storage/meter_nvm.h"
/** @brief 启动板级 NVM worker；调用前已初始化 Core，返回后由 App poll 接收加载结果。 */
bool meter_board_nvm_start(meter_core_t *core);
/** @brief App 唯一 owner 处理结果、观察设置并调度保存；没有阻塞存储 I/O。 */
void meter_board_nvm_poll(uint32_t now, meter_diag_storage_t *status);
/** @brief App 通知设置变化；只复制 RAM，不等待保存。 */
bool meter_board_nvm_changed(uint32_t now);
/** @brief App 查询启动加载是否已结束，避免迟到加载覆盖用户修改。 */
bool meter_board_nvm_ready(void);
/** @brief OTA/生命周期以具体版本申请保存并检查 durable；均只能从 App 调用。 */
uint64_t meter_board_nvm_flush(void);
bool meter_board_nvm_barrier(uint64_t target);
/** @brief App 显式重读并重试失败记录，不覆盖当前 RAM。 */
bool meter_board_nvm_retry(void);
/** @brief App 在 durable 且无在途 I/O 后请求退出，不强杀 worker。 */
void meter_board_nvm_stop(void);
/** @brief App 处理退出结果后返回 true；等待时资源保持有效。 */
bool meter_board_nvm_stopped(void);
#endif
