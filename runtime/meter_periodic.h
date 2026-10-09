#ifndef METER_PERIODIC_RUNTIME_H
#define METER_PERIODIC_RUNTIME_H
#include "contracts/meter_periodic.h"
/** @brief Protocol 独占状态；TX worker 不读写此结构。 */
typedef struct
{
    meter_deadline_t deadline;
    uint32_t generation, wire, proposed_wire;
    uint64_t ticket, pending_ticket;
    bool busy;
    uint32_t skipped, stale, rejected, completed, failed;
} meter_periodic_state_t;
/** @brief App 持短锁更新 publication；失败不修改，数组独立、容量充足。 */
bool meter_tx_publish(meter_tx_publication_t *publication, size_t capacity, const meter_tx_value_t *values,
                      size_t count, uint32_t generation, uint32_t now);
/** @brief 持 publication 短锁深复制；目标数组必须独立且容量充足。 */
bool meter_tx_copy(meter_tx_publication_t *destination, size_t capacity,
                   const meter_tx_publication_t *source);
/** @brief 检查 Product 周期配置及语义依赖范围，不执行 encoder。 */
bool meter_periodic_valid(const meter_periodic_frame_t *definition, size_t value_count);
/** @brief Protocol 新代次复位 wire/deadline；旧结果因 generation 不同被拒绝。 */
bool meter_periodic_reset(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                          uint32_t generation, uint32_t now);
/** @brief 到期只准备一帧；按计划跳过历史周期，忙槽不积累 backlog。 */
bool meter_periodic_prepare(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                            const meter_tx_publication_t *publication, bool allowed, uint32_t now,
                            meter_periodic_message_t *message);
/** @brief Protocol 提交队列准入；失败不推进 wire。 */
bool meter_periodic_admit(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                          const meter_periodic_message_t *message);
/** @brief Protocol 消费当前身份；失败、取消和旧结果不得推进完成型 wire。 */
bool meter_periodic_complete(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                             const meter_periodic_result_t *result);
/** @brief TX owner 在驱动调用前校验代次和期限，不证明物理 wire 时间。 */
bool meter_periodic_sendable(const meter_periodic_message_t *message, uint32_t generation, uint32_t now);
#endif
