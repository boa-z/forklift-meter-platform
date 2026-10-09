#ifndef METER_EXECUTION_RUNTIME_H
#define METER_EXECUTION_RUNTIME_H
#include "contracts/meter_execution.h"
/** @brief App 所有的生命周期；generation 隔离模式切换前的业务队列。 */
typedef struct
{
    meter_execution_state_t state;
    meter_mode_t mode;
    uint32_t generation;
} meter_execution_t;
/** @brief 启动期初始化；非空调用同步完成，无阻塞、分配或外部副作用。 */
void meter_execution_init(meter_execution_t *execution);
/** @brief App 检查状态迁移；非法迁移不修改对象，FAILED 仍需协作 STOPPING。 */
bool meter_execution_transition(meter_execution_t *execution, meter_execution_state_t next);
/** @brief App 设置运行模式；代数耗尽拒绝切换，禁止复用旧消息身份。 */
bool meter_execution_mode(meter_execution_t *execution, meter_mode_t mode);
/** @brief 返回保守默认策略；Product 可明确覆盖。无共享可变状态。 */
meter_mode_policy_t meter_execution_policy(meter_mode_t mode);
/** @brief Protocol 初始化期限；period 非零且小于半周期，否则对象不变。 */
bool meter_deadline_arm(meter_deadline_t *deadline, uint32_t now, uint32_t period);
/** @brief 到期只执行一次，按计划推进并记录跳过的周期；不补发突发、不累积漂移。 */
bool meter_deadline_take(meter_deadline_t *deadline, uint32_t now);
#endif
