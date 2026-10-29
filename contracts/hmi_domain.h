#ifndef HMI_DOMAIN_H
#define HMI_DOMAIN_H
#include "contracts/hmi_profile.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/**
 * @brief 数据有效性状态。
 *
 * VALID 表示值在时效门限内；STALE 表示值仍可读但已超时；UNKNOWN 表示从未收到有效数据；
 * ERROR 表示协议侧判定该信号不可用。UI 必须按状态渲染，不得只看数值。
 */
typedef enum
{
    HMI_VALUE_UNKNOWN,
    HMI_VALUE_VALID,
    HMI_VALUE_STALE,
    HMI_VALUE_ERROR
} hmi_value_state_t;
/** @brief 显示语言。取值同时是持久化格式中的语言字节，禁止重新排序。 */
typedef enum
{
    HMI_LANGUAGE_EN = 0,
    HMI_LANGUAGE_ZH = 1
} hmi_language_t;
/** @brief 本机 CAN 速率选择；取值即持久化字节，禁止重新排序。重启生效，所有已打开的总线共用。 */
typedef enum
{
    HMI_CAN_RATE_125K = 0,
    HMI_CAN_RATE_250K = 1,
    HMI_CAN_RATE_500K = 2
} hmi_can_rate_t;
/** @brief Product 启动默认设置；仅在初始化时使用，之后由持久化恢复覆盖。 */
typedef struct
{
    hmi_language_t language;
    hmi_can_rate_t can_rate;
    uint8_t brightness;
    bool imperial;
} hmi_initial_settings_t;
/**
 * @brief 信号与来源身份，产品词汇表中的 16 位句柄。
 *
 * 身份由产品目录解析，绝不是平台数组的下标，因此产品增删条目不需要修改 contracts 或 core。
 * 0 保留给“无此条目”，其余编号由 Product 分配。
 */
typedef uint16_t hmi_signal_id_t;
typedef uint16_t hmi_source_id_t;
#define HMI_SOURCE_NONE 0u
/** @brief 私有扩展身份区间的起点，公共目录不得占用。 */
#define HMI_ID_PRIVATE_FIRST 0x1000u
/** @brief 一次采样。timestamp_ms 为单调回绕的毫秒计数，非有限浮点一律降级为 ERROR。 */
typedef struct
{
    float value;
    uint32_t timestamp_ms;
    hmi_value_state_t state;
    hmi_source_id_t source;
} hmi_value_t;
/** @brief 协议解码写入 core 的最小单元：身份加值。 */
typedef struct
{
    hmi_signal_id_t signal;
    hmi_value_t value;
} hmi_update_t;
/** @brief 目录中的信号定义。id 是跨版本引用，key 是稳定字符串标识。 */
typedef struct
{
    const char *key;
    const char *unit;
    uint32_t stale_ms;
    hmi_signal_id_t id;
} hmi_signal_def_t;
/** @brief 以完整值查看当前与候选来源，返回 true 才接受候选值。 */
typedef bool (*hmi_source_policy_fn_t)(void *context, hmi_signal_id_t signal,
                                         const hmi_value_t *incoming, const hmi_value_t *current);
/**
 * @brief 本地权威 Settings 定义；不是远端 owner-qualified Parameter 描述符。
 *
 * min、max、initial 使用该参数自身单位，三者必须有限且 initial 落在 [min, max] 内，
 * 范围由产品目录给出，平台不做单位假设。
 */
typedef struct
{
    const char *key;
    const char *unit;
    float min, max, initial;
    uint16_t id;
} hmi_parameter_def_t;
/** @brief 故障定义。description 仅用于演示与诊断输出，不参与判定。 */
typedef struct
{
    uint16_t id;
    const char *key;
    const char *description;
} hmi_fault_def_t;
/** @brief 产品目录。core 只按数量校验并解析身份，不假设任何上限；各表都可以为空。
 *  页面怎样排列信号（监视页、列表）属于 Product 的 UI，不在目录里。 */
typedef struct
{
    const hmi_signal_def_t *signals;
    size_t signal_count;
    const hmi_parameter_def_t *parameters;
    size_t parameter_count;
    const hmi_fault_def_t *faults;
    size_t fault_count;
    hmi_source_policy_fn_t source_policy;
    void *source_policy_context;
    /** @brief 产品可选的语言数，ID 从 0 起连续且含义由 Product 定义；0 沿用默认的两种语言。 */
    uint8_t language_count;
} hmi_catalog_t;
/** @brief 显式声明的数量（包括单语言）；未声明时保留旧目录的 EN/ZH 默认值。 */
static inline unsigned hmi_catalog_language_count(const hmi_catalog_t *catalog)
{
    return catalog && catalog->language_count ? catalog->language_count : 2u;
}
/**
 * @brief 单个故障的状态记录，每个目录条目一条。
 *
 * 本阶段只保留激活位；后续如需来源或首次/末次时间，在此追加字段即可，
 * 不再退回把整表压缩进一个位图字。
 */
typedef struct
{
    uint16_t id;
    bool active;
} hmi_fault_state_t;
/**
 * @brief 域内存绑定：数量与容量由产品决定，平台不分配堆。
 *
 * 数组必须比绑定它的 hmi_core_t 活得更久（静态存储或调用方自有生命周期）。
 * hmi_core_init 会在写入任何元素前校验目录数量不超过容量。
 */
typedef struct
{
    hmi_value_t *signals;
    size_t signal_capacity;
    float *parameters;
    size_t parameter_capacity;
    hmi_fault_state_t *faults;
    size_t fault_capacity;
} hmi_core_storage_t;
/**
 * @brief UI 只读快照。
 *
 * signals、parameters、faults 与目录顺序一致，但读取必须走按身份的访问函数，
 * 不要把身份当作下标索引。generation 由 runtime 的连接代数推进。
 */
typedef struct
{
    const hmi_catalog_t *catalog;
    hmi_value_t *signals;
    float *parameters;
    hmi_fault_state_t *faults;
    uint32_t generation;
    uint32_t revision;
    bool connected;
    bool imperial;
    hmi_language_t language;
    uint8_t brightness;
    hmi_can_rate_t can_rate; /* 已配置值，不代表当前运行中的控制器速率。 */
    hmi_profile_t profile; /* 与 Domain 值一起在同一发布锁内复制。 */
} hmi_snapshot_t;
/** @brief 未声明身份的统一返回值：UNKNOWN 且数值与时间戳为零。 */
static inline hmi_value_t hmi_value_unknown(void)
{
    const hmi_value_t missing = {0, 0, HMI_VALUE_UNKNOWN, HMI_SOURCE_NONE};
    return missing;
}
/** @brief 身份到数组位置的解析；未声明时返回 signal_count，调用方据此判定失败。 */
static inline size_t hmi_catalog_index(const hmi_catalog_t *catalog, hmi_signal_id_t id)
{
    for (size_t i = 0; i < catalog->signal_count; ++i)
        if (catalog->signals[i].id == id)
            return i;
    return catalog->signal_count;
}
/** @brief 参数身份到数组位置的解析；未声明时返回 parameter_count。 */
static inline size_t hmi_catalog_parameter_index(const hmi_catalog_t *catalog, uint16_t id)
{
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        if (catalog->parameters[i].id == id)
            return i;
    return catalog->parameter_count;
}
/** @brief 故障身份到数组位置的解析；未声明时返回 fault_count。 */
static inline size_t hmi_catalog_fault_index(const hmi_catalog_t *catalog, uint16_t id)
{
    for (size_t i = 0; i < catalog->fault_count; ++i)
        if (catalog->faults[i].id == id)
            return i;
    return catalog->fault_count;
}
/* 读取是全覆盖的：产品未声明的身份只会得到 UNKNOWN，不会索引到别人的槽位。 */
static inline hmi_value_t hmi_snapshot_read(const hmi_snapshot_t *snapshot, hmi_signal_id_t id)
{
    if (!snapshot || !snapshot->catalog || !snapshot->signals)
        return hmi_value_unknown();
    size_t index = hmi_catalog_index(snapshot->catalog, id);
    return index < snapshot->catalog->signal_count ? snapshot->signals[index] : hmi_value_unknown();
}
/** @brief 按身份读参数；身份未声明或 out 为空时返回 false 且不写 *out。 */
static inline bool hmi_snapshot_parameter(const hmi_snapshot_t *snapshot, uint16_t id, float *out)
{
    if (!snapshot || !snapshot->catalog || !snapshot->parameters || !out)
        return false;
    size_t index = hmi_catalog_parameter_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->parameter_count)
        return false;
    *out = snapshot->parameters[index];
    return true;
}
/** @brief 按身份读故障激活状态；身份未声明视为未激活。 */
static inline bool hmi_snapshot_fault_active(const hmi_snapshot_t *snapshot, uint16_t id)
{
    if (!snapshot || !snapshot->catalog || !snapshot->faults)
        return false;
    size_t index = hmi_catalog_fault_index(snapshot->catalog, id);
    return index < snapshot->catalog->fault_count && snapshot->faults[index].active;
}
/** @brief 按身份写故障状态；身份未声明返回 false 且不写内存。只有产品策略应当调用。 */
static inline bool hmi_snapshot_fault_set(hmi_snapshot_t *snapshot, uint16_t id, bool active)
{
    if (!snapshot || !snapshot->catalog || !snapshot->faults)
        return false;
    size_t index = hmi_catalog_fault_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->fault_count)
        return false;
    if (snapshot->faults[index].active != active)
    {
        snapshot->faults[index].active = active;
        ++snapshot->revision;
    }
    return true;
}
/**
 * @brief App 发布运行中识别到的设备型号配置（系列与能力位），例如所连接控制器的型号。
 *
 * 未确认时系列与能力必须为零，已确认时系列非零，否则返回 false 且不改快照。内容不变时不推进代次；
 * 语义变化推进 profile.generation 与快照 revision，代次耗尽时拒绝回绕。只在运行时线程调用。
 */
static inline bool hmi_snapshot_profile(hmi_snapshot_t *snapshot, bool confirmed, uint16_t family,
                                          uint64_t capabilities)
{
    if (!snapshot || (confirmed && family == 0u) || (!confirmed && (family != 0u || capabilities != 0u)))
        return false;
    const hmi_profile_t current = snapshot->profile;
    if (current.confirmed == confirmed && current.family == family && current.capabilities == capabilities)
        return true;
    if (current.generation == UINT32_MAX)
        return false;
    snapshot->profile = (hmi_profile_t){current.generation + 1u, family, capabilities, confirmed};
    ++snapshot->revision;
    return true;
}
/**
 * @brief 运行时线程里的 Product 业务（app_tick、local_action、适配器）发布一个本地算出的信号值：
 * 写成 VALID 并带上 now_ms；数值或状态变化时递增 revision。身份未声明返回 false。
 */
static inline bool hmi_snapshot_write(hmi_snapshot_t *snapshot, hmi_signal_id_t id, float value, uint32_t now_ms)
{
    if (!snapshot || !snapshot->catalog || !snapshot->signals)
        return false;
    const size_t index = hmi_catalog_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->signal_count)
        return false;
    hmi_value_t *slot = &snapshot->signals[index];
    if (slot->state != HMI_VALUE_VALID || slot->value != value)
        ++snapshot->revision;
    *slot = (hmi_value_t){value, now_ms, HMI_VALUE_VALID, HMI_SOURCE_NONE};
    return true;
}
/** @brief UI 本地设置意图。HMI_ACTION_PARAMETER 使用本地 id，不派发远端事务。其余类别忽略 id。 */
typedef enum
{
    HMI_ACTION_UNITS,
    HMI_ACTION_BRIGHTNESS,
    HMI_ACTION_PARAMETER,
    HMI_ACTION_LANGUAGE,
    /* 可选 Product 语义意图；沿用有界动作队列，不解释为本机参数编号。 */
    HMI_ACTION_PRODUCT
} hmi_action_kind_t;
/** @brief UI 发起的动作；value 的含义由 kind 决定，参数动作使用参数自身单位。 */
typedef struct
{
    hmi_action_kind_t kind;
    uint16_t id;
    float value;
} hmi_action_t;
/**
 * @brief UI 提交语义意图的动作回调，在 UI 线程执行。
 *
 * 返回 true 只表示已排入运行时线程的动作队列，结果以之后发布的快照为准；返回 false 表示队列已满或
 * 运行时不接收设置（升级维护期间）。实现不得在 UI 线程等待介质写入。
 */
typedef bool (*hmi_action_send_t)(void *context, const hmi_action_t *action);
/** @brief 动作回调与其不透明上下文，由产品 UI 在创建时持有。 */
typedef struct
{
    hmi_action_send_t send;
    void *context;
} hmi_ui_actions_t;
#endif
