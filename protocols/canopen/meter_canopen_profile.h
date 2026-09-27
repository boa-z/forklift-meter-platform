#ifndef METER_CANOPEN_PROFILE_H
#define METER_CANOPEN_PROFILE_H

/**
 * @file meter_canopen_profile.h
 * @brief 平台侧 feature-selectable CANopen capabilities；不复制 CANopen 标准实现。
 *
 * Platform 只描述能力开关，协议策略由 Product 选择。历史全局拒绝 NMT/Heartbeat
 * 的 meter_canopen_profile_valid() 已改为能力有效性校验，产品可自由组合。
 * Reference-Mixed 明确选择 PDO RX/TX + SDO Client，不启用 NMT/Heartbeat；
 * 未来产品可自行启用 NMT/Heartbeat。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 产品选择的 CANopen 能力组合。 */
typedef struct
{
    uint8_t node_id;
    bool pdo_rx;
    bool pdo_tx;
    bool sdo_client;
    bool sdo_server;
    bool nmt;
    bool heartbeat;
    bool emcy;
    bool sync;
} meter_canopen_profile_t;

/** @brief 平台级有效性：node_id 合法且至少启用一种传输能力；NMT/Heartbeat 可选。 */
static inline bool meter_canopen_profile_valid(const meter_canopen_profile_t *profile)
{
    if (profile == NULL || profile->node_id == 0 || profile->node_id > 127)
        return false;
    return profile->pdo_rx || profile->pdo_tx || profile->sdo_client || profile->sdo_server ||
           profile->nmt || profile->heartbeat || profile->emcy || profile->sync;
}

/** @brief Reference-Mixed 及同类 PDO/SDO-only 产品的不变量：无需 NMT/Heartbeat 即可在线。 */
static inline bool meter_canopen_profile_pdo_sdo_only(const meter_canopen_profile_t *profile)
{
    return meter_canopen_profile_valid(profile) && (profile->pdo_rx || profile->pdo_tx || profile->sdo_client) &&
           !profile->nmt && !profile->heartbeat;
}

#endif
