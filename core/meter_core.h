#ifndef METER_CORE_H
#define METER_CORE_H
#include "contracts/meter_domain.h"
#include "contracts/meter_batch.h"
#include "diagnostics/meter_diagnostics.h"
/**
 * @brief 域内核：只读快照加上它所绑定的产品存储。
 *
 * 存储数组属于调用方，必须比 core 活得更久；core 只做绑定与容量校验，不分配内存。
 * 复制该结构体会让两个副本共享同一组数组，嵌入式代码不应依赖这种别名行为。
 */
typedef struct
{
    meter_snapshot_t snapshot;
    meter_diagnostics_t *diag;
    meter_core_storage_t storage;
} meter_core_t;
/**
 * @brief 绑定目录与存储，并把信号复位为 UNKNOWN、参数复位为 initial、故障复位为未激活。
 *
 * 目录不合法（身份为零或重复、监视项无法解析、参数范围异常）或任一容量不足时返回 false，
 * 此时既不写 core 也不写 storage。仅在启动阶段调用一次，不阻塞、不持锁。
 */
bool meter_core_init(meter_core_t *core, const meter_catalog_t *catalog, const meter_core_storage_t *storage);
/**
 * @brief 写入一个信号值，作为 meter_update_sink_t 使用。
 *
 * 身份未声明返回 false；非有限浮点降级为 ERROR。在解码方所在线程执行，宿主实现与 UI 同线程。
 */
bool meter_core_apply(void *context, const meter_update_t *update);
/** @brief App 独占调用；先验证整批身份、代数、来源仲裁，再一次提交。
 * @details 失败不修改 Domain；策略必须为纯判定。同步无 I/O，不持有 batch 指针。
 */
bool meter_core_apply_batch(meter_core_t *core, const meter_update_batch_t *batch);
/** @brief 按目录中每个信号自己的 stale_ms 降级；0 表示不自动超时。 */
void meter_core_tick(meter_core_t *core, uint32_t now_ms);
/** @brief 更新连接状态与代数；断开时立即把所有 VALID 降级为 STALE，并保留最后可读值。 */
void meter_core_connection(meter_core_t *core, bool connected, uint32_t generation);
/** @brief 应用 UI 设置动作；越界或未知类别返回 false 且不改变状态。在 UI 线程执行。 */
bool meter_core_action(meter_core_t *core, const meter_action_t *action);
/** @brief 只校验不写入：按身份检查参数值域，单位与范围由产品目录定义。 */
bool meter_core_parameter_valid(const meter_core_t *core, uint16_t id, float value);
/** @brief 校验通过后写入参数；失败时不改变现值。 */
bool meter_core_parameter(meter_core_t *core, uint16_t id, float value);
/** @brief 返回只读快照，其生命周期与 core 相同。 */
const meter_snapshot_t *meter_core_snapshot(const meter_core_t *core);
/** @brief 绑定诊断及公共 Domain 视图；目录/存储/诊断实例须覆盖绑定期，owner 线程调用。 */
void meter_core_bind_diagnostics(meter_core_t *core, meter_diagnostics_t *diag);
#endif
