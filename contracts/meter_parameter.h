#ifndef METER_PARAMETER_H
#define METER_PARAMETER_H
#include "contracts/meter_request.h"

/** Product 定义的逻辑身份，不是 CAN ID、SDO 地址或持久化偏移。 */
typedef struct
{
    uint16_t owner; /* 非零逻辑端点。 */
    uint16_t id;
} meter_parameter_key_t;

static inline bool meter_parameter_key_equal(meter_parameter_key_t first, meter_parameter_key_t second)
{
    return (first.owner == second.owner) && (first.id == second.id);
}

typedef enum
{
    METER_PARAMETER_READ,
    METER_PARAMETER_WRITE
} meter_parameter_operation_t;

/** 不可变参数目录只能由 Product 包定义，框架不提供车辆参数或默认目录。
 * 零初始化表示未确认。数值采用工程单位浮点数，精确线上表示由编解码器负责。
 * 需要无损宽整数或数据块时，必须另行评审扩展。 */
typedef struct
{
    meter_parameter_key_t key;
    bool confirmed;
    bool readable;
    bool writable;
    bool repeatable_read; /* Product 确认重读不会重复产生副作用。 */
    float minimum;
    float maximum;
    uint32_t read_permissions;
    uint32_t write_permissions; /* 已确认的可写条目必须声明非零权限。 */
} meter_parameter_definition_t;

typedef struct
{
    uint32_t total_timeout_ms;   /* 从准入开始，包含排队时间。 */
    uint32_t attempt_timeout_ms; /* 从复制请求交给后端开始。 */
    uint8_t max_attempts;        /* 写入只能尝试一次；读取可显式允许重试。 */
} meter_parameter_policy_t;

typedef struct
{
    meter_request_id_t id;
    meter_parameter_key_t key;
    meter_parameter_operation_t operation;
    float value;     /* 仅 WRITE 使用。 */
    uint8_t attempt; /* 从 1 开始，由校验后的后端原样返回。 */
} meter_parameter_work_t;

typedef enum
{
    METER_PARAMETER_REPLY_OK,
    METER_PARAMETER_REPLY_REJECTED,
    METER_PARAMETER_REPLY_TRANSPORT_FAILED
} meter_parameter_reply_code_t;

typedef struct
{
    meter_parameter_work_t request;
    meter_parameter_reply_code_t code;
    bool has_value;
    float value;
    int32_t detail; /* 后端诊断详情，不是健康模式。 */
} meter_parameter_reply_t;

/** 可选的 App 非阻塞复制消息端口；事务核心不保存或调用此端口。
 * Product 负责绑定后端，context 及回调在 App 调用期间必须有效。
 * ready 表示后端有接收额度且旧请求已排空/隔离，不代表事务结果已领取。
 * send 成功前必须复制 work，不得保留其地址；失败不允许自动重发写入。
 * receive 成功时复制并消费一个完整回复；失败时没有回复可取。
 * 回调不得阻塞、内联执行设备 I/O 或运行其他所有者。存储/传输工作由
 * 各自所有者执行，再通过复制回复报告完成；需耐久性时必须等到存储确认。
 * 取消事务或确认领取结果不会释放后端资源。 */
typedef struct
{
    void *context;
    bool (*ready)(void *context);
    bool (*send)(void *context, const meter_parameter_work_t *work);
    bool (*receive)(void *context, meter_parameter_reply_t *reply);
} meter_parameter_exchange_t;

typedef enum
{
    METER_PARAMETER_SUCCEEDED,
    METER_PARAMETER_TIMED_OUT,
    METER_PARAMETER_PERMISSION_LOST,
    METER_PARAMETER_CANCELLED,
    METER_PARAMETER_REMOTE_REJECTED,
    METER_PARAMETER_INVALID_REPLY,
    METER_PARAMETER_TRANSPORT_FAILED
} meter_parameter_outcome_t;

typedef struct
{
    meter_parameter_work_t request;
    meter_parameter_outcome_t outcome;
    bool effect_unknown; /* 已派发的写入可能已生效，不声称已经回滚。 */
    bool has_value;
    float value;
    int32_t detail;
} meter_parameter_result_t;

typedef enum
{
    METER_PARAMETER_ACCEPTED,
    METER_PARAMETER_BUSY,
    METER_PARAMETER_INVALID,
    METER_PARAMETER_NOT_FOUND,
    METER_PARAMETER_UNCONFIRMED,
    METER_PARAMETER_DENIED,
    METER_PARAMETER_OUT_OF_RANGE,
    METER_PARAMETER_EXHAUSTED
} meter_parameter_admission_t;
#endif
