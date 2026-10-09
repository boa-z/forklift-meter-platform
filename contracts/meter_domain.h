#ifndef METER_DOMAIN_H
#define METER_DOMAIN_H
#include "contracts/meter_profile.h"
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
    METER_VALUE_UNKNOWN,
    METER_VALUE_VALID,
    METER_VALUE_STALE,
    METER_VALUE_ERROR
} meter_value_state_t;
/** @brief 显示语言。取值同时是持久化格式中的语言字节，禁止重新排序。 */
typedef enum
{
    METER_LANGUAGE_EN = 0,
    METER_LANGUAGE_ZH = 1
} meter_language_t;
/** @brief 本机 CAN 速率选择；持久化取值固定，两个参考板总线共用，重启生效。 */
typedef enum
{
    METER_CAN_RATE_125K = 0,
    METER_CAN_RATE_250K = 1,
    METER_CAN_RATE_500K = 2
} meter_can_rate_t;
/** @brief Product 启动默认设置；仅在初始化时使用，之后由持久化恢复覆盖。 */
typedef struct
{
    meter_language_t language;
    meter_can_rate_t can_rate;
    uint8_t brightness;
    bool imperial;
} meter_initial_settings_t;
/**
 * @brief 信号与来源身份，产品词汇表中的 16 位句柄。
 *
 * 身份由产品目录解析，绝不是平台数组的下标，因此产品增删条目不需要修改 contracts 或 core。
 * 公共 Demo 身份从 1 开始；0 保留给“无此条目”。
 */
typedef uint16_t meter_signal_id_t;
typedef uint16_t meter_source_id_t;
#define METER_SOURCE_NONE 0u
#define METER_SOURCE_DEMO 1u
/** @brief 私有扩展身份区间的起点，公共目录不得占用。 */
#define METER_ID_PRIVATE_FIRST 0x1000u
/** @brief 一次采样。timestamp_ms 为单调回绕的毫秒计数，非有限浮点一律降级为 ERROR。 */
typedef struct
{
    float value;
    uint32_t timestamp_ms;
    meter_value_state_t state;
    meter_source_id_t source;
} meter_value_t;
/** @brief 协议解码写入 core 的最小单元：身份加值。 */
typedef struct
{
    meter_signal_id_t signal;
    meter_value_t value;
} meter_update_t;
/** @brief 目录中的信号定义。id 是跨版本引用，key 是稳定字符串标识。 */
typedef struct
{
    const char *key;
    const char *unit;
    uint32_t stale_ms;
    meter_signal_id_t id;
} meter_signal_def_t;
/** @brief 以完整值查看当前与候选来源，返回 true 才接受候选值。 */
typedef bool (*meter_source_policy_fn_t)(void *context, meter_signal_id_t signal,
                                         const meter_value_t *incoming, const meter_value_t *current);
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
} meter_parameter_def_t;
/** @brief 监视页的一行呈现映射。signal 必须指向已声明的信号身份，本身不占域存储。 */
typedef struct
{
    const char *key;
    const char *unit;
    meter_signal_id_t signal;
} meter_monitor_def_t;
/** @brief 故障定义。description 仅用于演示与诊断输出，不参与判定。 */
typedef struct
{
    uint16_t id;
    const char *key;
    const char *description;
} meter_fault_def_t;
/** @brief 产品目录。core 只按数量校验并解析身份，不假设任何上限。 */
typedef struct
{
    const meter_signal_def_t *signals;
    size_t signal_count;
    const meter_parameter_def_t *parameters;
    size_t parameter_count;
    const meter_monitor_def_t *monitors;
    size_t monitor_count;
    const meter_fault_def_t *faults;
    size_t fault_count;
    meter_source_policy_fn_t source_policy;
    void *source_policy_context;
} meter_catalog_t;
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
} meter_fault_state_t;
/**
 * @brief 域内存绑定：数量与容量由产品决定，平台不分配堆。
 *
 * 数组必须比绑定它的 meter_core_t 活得更久（静态存储或调用方自有生命周期）。
 * meter_core_init 会在写入任何元素前校验目录数量不超过容量。
 */
typedef struct
{
    meter_value_t *signals;
    size_t signal_capacity;
    float *parameters;
    size_t parameter_capacity;
    meter_fault_state_t *faults;
    size_t fault_capacity;
} meter_core_storage_t;
/**
 * @brief UI 只读快照。
 *
 * signals、parameters、faults 与目录顺序一致，但读取必须走按身份的访问函数，
 * 不要把身份当作下标索引。generation 由 runtime 的连接代数推进。
 */
typedef struct
{
    const meter_catalog_t *catalog;
    meter_value_t *signals;
    float *parameters;
    meter_fault_state_t *faults;
    uint32_t generation;
    uint32_t revision;
    bool connected;
    bool imperial;
    meter_language_t language;
    uint8_t brightness;
    meter_can_rate_t can_rate; /* 已配置值，不代表当前运行中的控制器速率。 */
    meter_profile_t profile; /* 与 Domain 值一起在同一发布锁内复制。 */
} meter_snapshot_t;
/** @brief 未声明身份的统一返回值：UNKNOWN 且数值与时间戳为零。 */
static inline meter_value_t meter_value_unknown(void)
{
    const meter_value_t missing = {0, 0, METER_VALUE_UNKNOWN, METER_SOURCE_NONE};
    return missing;
}
/** @brief 身份到数组位置的解析；未声明时返回 signal_count，调用方据此判定失败。 */
static inline size_t meter_catalog_index(const meter_catalog_t *catalog, meter_signal_id_t id)
{
    for (size_t i = 0; i < catalog->signal_count; ++i)
        if (catalog->signals[i].id == id)
            return i;
    return catalog->signal_count;
}
/** @brief 参数身份到数组位置的解析；未声明时返回 parameter_count。 */
static inline size_t meter_catalog_parameter_index(const meter_catalog_t *catalog, uint16_t id)
{
    for (size_t i = 0; i < catalog->parameter_count; ++i)
        if (catalog->parameters[i].id == id)
            return i;
    return catalog->parameter_count;
}
/** @brief 故障身份到数组位置的解析；未声明时返回 fault_count。 */
static inline size_t meter_catalog_fault_index(const meter_catalog_t *catalog, uint16_t id)
{
    for (size_t i = 0; i < catalog->fault_count; ++i)
        if (catalog->faults[i].id == id)
            return i;
    return catalog->fault_count;
}
/* 读取是全覆盖的：产品未声明的身份只会得到 UNKNOWN，不会索引到别人的槽位。 */
static inline meter_value_t meter_snapshot_read(const meter_snapshot_t *snapshot, meter_signal_id_t id)
{
    if (!snapshot || !snapshot->catalog || !snapshot->signals)
        return meter_value_unknown();
    size_t index = meter_catalog_index(snapshot->catalog, id);
    return index < snapshot->catalog->signal_count ? snapshot->signals[index] : meter_value_unknown();
}
/** @brief 按身份读参数；身份未声明或 out 为空时返回 false 且不写 *out。 */
static inline bool meter_snapshot_parameter(const meter_snapshot_t *snapshot, uint16_t id, float *out)
{
    if (!snapshot || !snapshot->catalog || !snapshot->parameters || !out)
        return false;
    size_t index = meter_catalog_parameter_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->parameter_count)
        return false;
    *out = snapshot->parameters[index];
    return true;
}
/** @brief 按身份读故障激活状态；身份未声明视为未激活。 */
static inline bool meter_snapshot_fault_active(const meter_snapshot_t *snapshot, uint16_t id)
{
    if (!snapshot || !snapshot->catalog || !snapshot->faults)
        return false;
    size_t index = meter_catalog_fault_index(snapshot->catalog, id);
    return index < snapshot->catalog->fault_count && snapshot->faults[index].active;
}
/** @brief 按身份写故障状态；身份未声明返回 false 且不写内存。只有产品策略应当调用。 */
static inline bool meter_snapshot_fault_set(meter_snapshot_t *snapshot, uint16_t id, bool active)
{
    if (!snapshot || !snapshot->catalog || !snapshot->faults)
        return false;
    size_t index = meter_catalog_fault_index(snapshot->catalog, id);
    if (index >= snapshot->catalog->fault_count)
        return false;
    if (snapshot->faults[index].active != active)
    {
        snapshot->faults[index].active = active;
        ++snapshot->revision;
    }
    return true;
}
/** @brief UI 本地设置意图。METER_ACTION_PARAMETER 使用本地 id，不派发远端事务。其余类别忽略 id。 */
typedef enum
{
    METER_ACTION_UNITS,
    METER_ACTION_BRIGHTNESS,
    METER_ACTION_PARAMETER,
    METER_ACTION_LANGUAGE,
    /* 可选 Product 语义意图；沿用有界动作队列，不解释为本机参数编号。 */
    METER_ACTION_PRODUCT
} meter_action_kind_t;
/** @brief UI 发起的动作；value 的含义由 kind 决定，参数动作使用参数自身单位。 */
typedef struct
{
    meter_action_kind_t kind;
    uint16_t id;
    float value;
} meter_action_t;
/**
 * @brief UI 提交语义意图的动作回调，在 UI 线程执行。
 *
 * 生产端返回 true 仅表示 QUEUED，不代表 APPLIED；最终以 App 结果/快照为准。返回 false
 * 表示未接纳。实现若需持久化，只投递请求，不得在 UI 线程 等待介质写入完成。
 */
typedef bool (*meter_action_send_t)(void *context, const meter_action_t *action);
/** @brief 动作回调与其不透明上下文，由产品 UI 在创建时持有。 */
typedef struct
{
    meter_action_send_t send;
    void *context;
} meter_ui_actions_t;
/**
 * @brief 持久化端口约定，供板级适配层实现。
 *
 * load 把已存字节写入 data 并回填 *size，容量不足返回 false；save 接受 encode 产出的
 * 完整字节串。实际介质写入必须在 UI 线程之外完成，避免阻塞渲染循环。
 */
typedef struct
{
    bool (*load)(void *context, uint8_t *data, size_t capacity, size_t *size);
    bool (*save)(void *context, const uint8_t *data, size_t size);
    void *context;
} meter_persistence_port_t;
#endif
