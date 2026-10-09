#ifndef METER_EXECUTION_BUDGET_H
#define METER_EXECUTION_BUDGET_H
/** @brief 板级静态 RAM 预算，可由构建定义覆盖；启动时校验 Product，不是平台模型上限。 */
#ifndef METER_BOARD_PERIODIC_SLOTS
#define METER_BOARD_PERIODIC_SLOTS 8u
#endif
/** @brief 每份语义副本的板级容量；App staging/publication/Protocol 各一份。 */
#ifndef METER_BOARD_TX_VALUES
#define METER_BOARD_TX_VALUES 16u
#endif
#endif
