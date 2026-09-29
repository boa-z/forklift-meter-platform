#ifndef REFERENCE_PARAMETER_PRESENTATION_H
#define REFERENCE_PARAMETER_PRESENTATION_H
#include "contracts/meter_parameter.h"
/* 合成 Product 的语义字段，不是远端属主或对象 ID。App 将意图映射到
 * 自身目录；UI 不选择线上地址。 */
typedef enum
{
    REFERENCE_FIELD_TRAVEL_LIMIT = 1,
    REFERENCE_FIELD_LIFT_LIMIT,
    REFERENCE_FIELD_AUXILIARY_LIMIT
} reference_parameter_field_t;
/* 仅包含 UI 展示值，不包含运行时、授权、传输、存储或可变指针。 */
typedef struct
{
    uint32_t profile_generation;
    uint64_t view_token; /* UI 为每次面板生命周期分配新的非零令牌。 */
    reference_parameter_field_t field;
    meter_parameter_operation_t operation;
    float value;
} reference_parameter_intent_t;
typedef struct
{
    uint32_t profile_generation;
    uint64_t view_token;
    meter_request_id_t request;
    bool pending;
    bool has_result;
    meter_parameter_outcome_t outcome; /* 仅在 has_result 为真时有效。 */
    bool effect_unknown;
    uint8_t attempts; /* 零表示尚未派发。 */
    bool has_value;
    float value; /* 仅在 has_value 为真时有效。 */
} reference_parameter_view_t;
/* 关闭或替换后的面板不得渲染前一个面板的完成结果。忽略展示值
 * 不等于确认领取结果，也不会取消后端操作。 */
static inline bool reference_parameter_view_matches(const reference_parameter_view_t *view,
                                                    uint32_t generation, uint64_t token)
{
    return view && token != 0u && view->profile_generation == generation && view->view_token == token;
}
#endif
