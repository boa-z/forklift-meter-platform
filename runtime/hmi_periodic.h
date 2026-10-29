#ifndef HMI_PERIODIC_RUNTIME_H
#define HMI_PERIODIC_RUNTIME_H
#include "contracts/hmi_periodic.h"
/** @brief 单调毫秒期限；周期必须小于半周期（contracts/hmi_time.h）。 */
typedef struct
{
    uint32_t period_ms, next_ms, missed, late_ms;
    bool armed;
} hmi_deadline_t;
/** @brief 从 now 起按 period 排第一个期限；period 为零或过大时返回 false 且不改对象。 */
bool hmi_deadline_arm(hmi_deadline_t *deadline, uint32_t now, uint32_t period);
/** @brief 到期只返回一次 true，按计划推进并记录跳过的周期；不补发、不累积漂移。 */
bool hmi_deadline_take(hmi_deadline_t *deadline, uint32_t now);
/** @brief Product 周期定义是否可用（总线、长度、周期）。 */
bool hmi_periodic_valid(const hmi_periodic_frame_t *definition);
/**
 * @brief 运行时线程每轮调用：期限到达且条目允许发送时，用 encode 从快照生成一帧。
 *
 * in_flight 为 true（上一帧还没被发送线程取走）时只推进期限、不再生成新帧，避免积压。
 * 返回 true 表示 *frame 已就绪，由调用方交给该总线的发送队列。
 */
bool hmi_periodic_due(hmi_deadline_t *deadline, const hmi_periodic_frame_t *definition,
                        const hmi_snapshot_t *snapshot, uint32_t now, bool allowed, bool in_flight,
                        hmi_can_frame_t *frame);
#endif
