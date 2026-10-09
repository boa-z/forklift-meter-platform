#ifndef METER_EXECUTION_PORT_H
#define METER_EXECUTION_PORT_H
#include "core/meter_core.h"
#include "contracts/meter_product.h"
#include "contracts/meter_request.h"
/** @brief 初始化绑定；全部存储静态且互不重叠，发布副本不能引用 Core 数组。 */
typedef struct
{
    const meter_product_t *product;
    meter_core_t *core;
    meter_core_storage_t published_storage, diagnostic_storage;
    meter_diagnostics_t *public_diagnostics;
} meter_execution_config_t;
/** @brief UI 启动期调用一次；初始化 IPC 后启动 owner，输入由 Protocol 最后打开。 */
bool meter_execution_start(const meter_execution_config_t *config);
/** @brief UI 提交值意图；true 仅表示 QUEUED，App 可按模式拒绝，不代表业务成功。 */
bool meter_execution_action(void *context, const meter_action_t *action);
/** @brief App 提交单个在途业务请求；Product 路由，返回 QUEUED 不代表业务成功。 */
meter_request_admission_t meter_execution_command(const meter_command_t *command, uint32_t timeout_ms,
                                                  meter_request_id_t *id);
/** @brief 按身份查询值进度；ack 只允许消费终态，未消费前拒绝新请求。 */
bool meter_execution_command_result(meter_request_id_t id, meter_command_stage_t *stage, bool acknowledge);
/** @brief UI 取得本地设置结果并释放额度；未完成返回 false，不覆盖 out。 */
bool meter_execution_action_result(meter_request_result_t *out);
/** @brief UI 短锁深复制，离锁后独占副本；false 时禁止渲染新数据。 */
bool meter_execution_present(meter_snapshot_t *snapshot, const meter_core_storage_t *storage,
                              meter_update_view_t *update, bool *normal_ui);
/** @brief UI 发布自己的统计副本；不持锁执行 LVGL。 */
void meter_execution_ui_diagnostics(const meter_diagnostics_t *diagnostics);
/** @brief Protocol 的唯一有界 TX 入口；urgent 使用独立额度，不直接调用驱动。 */
bool meter_execution_can_submit(const meter_can_frame_t *frame, bool urgent);
/** @brief App/诊断提交停止意图；异步处理，超时不回收在途资源。 */
void meter_execution_stop(void);
/** @brief 最终停机状态，包含 UI 已释放资源并确认；UI 释放许可应查询下一个接口。 */
bool meter_execution_stopped(void);
/** @brief UI 查询其他 owner 已结束后是否可以释放 LVGL；此时尚未对外宣告 STOPPED。 */
bool meter_execution_ui_shutdown_requested(void);
/** @brief UI 在释放所有显示资源后确认退出；App 收到确认后才进入 STOPPED。 */
void meter_execution_ui_stopped(void);
/** @brief App/MSH 设置入口短锁查询统一模式准入；不读取可变 Core。 */
bool meter_execution_settings_allowed(void);
/**
 * @brief 看门狗监督用的只读采样；短锁复制，不返回指针、不修改运行期状态。
 *
 * started 表示 runtime 已完成初始化（owner 已建立）；stopping 表示停止流程进行中，
 * 此时禁止复位。owners_expected 是"常驻 owner 此刻应当推进"的唯一判据来源。
 * 未启动时返回 false，调用方据此保持非监督态。
 */
typedef struct
{
    uint32_t protocol_runs, app_runs, ui_ticks;
    bool started, owners_expected, stopping;
} meter_execution_liveness_t;
bool meter_execution_liveness(meter_execution_liveness_t *out);
#endif
