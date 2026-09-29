#include "core/meter_core.h"
#include "core/meter_snapshot.h"
#include "diagnostics/meter_classification.h"
#include "runtime/meter_calibration.h"
#include <assert.h>

/* 本文件是可执行的合成 Product 适配示例，不进入固件源清单。
 * 映射、缓存失效及授权撤销均为本示例策略，不代表真实车辆安全策略。 */
static const meter_signal_def_t signals[] = {{"capture", "V", 50, 41}};
static const meter_catalog_t catalog = {.signals = signals, .signal_count = 1};
static const meter_parameter_definition_t parameters[] = {{.key = {2, 9},
                                                           .confirmed = true,
                                                           .readable = true,
                                                           .writable = true,
                                                           .minimum = 0,
                                                           .maximum = 100,
                                                           .read_permissions = 1,
                                                           .write_permissions = 1}};
static const meter_calibration_definition_t calibration = {.source = 41,
                                                           .target = {2, 9},
                                                           .maximum_age_ms = 50,
                                                           .verify = true,
                                                           .tolerance = 0.1f,
                                                           .write_policy = {100, 20, 1},
                                                           .read_policy = {100, 20, 1}};
#define CAPTURE_SUPPORTED UINT64_C(8)

typedef struct
{
    meter_core_t core;
    meter_value_t signals[1];
    meter_calibration_t calibration;
    meter_authorization_t authorization;
    uint32_t acquisition_generation;
    bool stationary;
    bool backend_busy;
    meter_parameter_work_t backend_work;
} product_app_t;

/* UI 只消费此值副本；不接触参数键、原始控制器身份或能力位。
 * 结果自带原始代数及令牌，历史不确定结果不会伪装成当前操作成功。 */
typedef struct
{
    uint32_t revision, profile_generation;
    float measurement;
    meter_value_state_t measurement_state;
    bool capture_supported, pending, has_result, result_current, effect_unknown;
    uint32_t result_generation;
    uint64_t result_token;
    meter_calibration_outcome_t outcome;
} product_view_t;

static void setup(product_app_t *app)
{
    *app = (product_app_t){.stationary = true};
    const meter_core_storage_t storage = {.signals = app->signals, .signal_capacity = 1};
    assert(meter_core_init(&app->core, &catalog, &storage));
    assert(meter_calibration_init(&app->calibration, parameters, 1, 39));
}
static bool capture_supported(const meter_profile_t *profile)
{
    return profile->confirmed && (profile->capabilities & CAPTURE_SUPPORTED) != 0;
}
static bool prerequisites(const product_app_t *app)
{
    return app->stationary && capture_supported(&app->core.snapshot.profile);
}
/* 只在 App 所有者内执行完整更新，再使用已有快照发布边界。
 * 0xA1、0x40 是合成测试输入，不是客户身份或线上协议定义。 */
static void replace_profile(product_app_t *app, unsigned identity, unsigned features, uint32_t now)
{
    uint32_t previous = app->core.snapshot.profile.generation;
    bool known = identity == 0xA1u;
    uint64_t capabilities = known && (features & 0x40u) ? CAPTURE_SUPPORTED : 0;
    assert(meter_core_profile(&app->core, known, known ? 17 : 0, capabilities));
    if (previous == app->core.snapshot.profile.generation)
        return;
    meter_update_t unavailable = {.signal = 41, .value = meter_value_unknown()};
    assert(meter_core_apply(&app->core, &unavailable));
    app->acquisition_generation = 0;
    meter_authorization_revoke(&app->authorization);
    meter_calibration_step(&app->calibration, &app->core.snapshot.profile, prerequisites(app),
                           &app->authorization, now);
}
static bool acquire(product_app_t *app, uint32_t generation, float value, uint32_t now)
{
    if (!meter_profile_matches(&app->core.snapshot.profile, generation))
        return false;
    meter_update_t sample = {.signal = 41, .value = {value, now, METER_VALUE_VALID, 1}};
    if (!meter_core_apply(&app->core, &sample))
        return false;
    app->acquisition_generation = generation;
    return true;
}
static product_view_t present(const product_app_t *app, uint64_t panel_token)
{
    meter_value_t values[1];
    meter_core_storage_t storage = {.signals = values, .signal_capacity = 1};
    meter_snapshot_t snapshot;
    assert(meter_snapshot_copy(&snapshot, &storage, &app->core.snapshot) == METER_SNAPSHOT_COPIED);
    meter_value_t sample = meter_snapshot_read(&snapshot, 41);
    meter_calibration_view_t result;
    meter_calibration_present(&app->calibration, &result);
    return (product_view_t){
        .revision = snapshot.revision,
        .profile_generation = snapshot.profile.generation,
        .measurement = sample.value,
        .measurement_state = sample.state,
        .capture_supported = capture_supported(&snapshot.profile),
        .pending = result.phase == METER_CALIBRATION_WRITING || result.phase == METER_CALIBRATION_READING,
        .has_result = result.phase == METER_CALIBRATION_DONE,
        .result_current = meter_calibration_view_matches(&result, &snapshot.profile, panel_token),
        .effect_unknown = result.effect_unknown,
        .result_generation = result.profile_generation,
        .result_token = result.token,
        .outcome = result.outcome};
}
static void begin(product_app_t *app, uint64_t token, uint32_t now)
{
    assert(meter_calibration_begin(&app->calibration, &calibration, &app->core.snapshot,
                                   app->acquisition_generation, token, prerequisites(app),
                                   &app->authorization, now));
}
/* 后端资源不随 App 结果领取而释放；忙碌期间仍推进服务期限，但不得调用 take。 */
static bool dispatch(product_app_t *app, uint32_t now)
{
    meter_calibration_step(&app->calibration, &app->core.snapshot.profile, prerequisites(app),
                           &app->authorization, now);
    if (app->backend_busy)
        return false;
    app->backend_busy =
        meter_calibration_take(&app->calibration, &app->core.snapshot.profile, prerequisites(app),
                               &app->authorization, now, &app->backend_work);
    return app->backend_busy;
}
static bool complete(product_app_t *app, float value, uint32_t now)
{
    assert(app->backend_busy);
    meter_parameter_reply_t reply = {
        .request = app->backend_work, .code = METER_PARAMETER_REPLY_OK, .has_value = true, .value = value};
    app->backend_busy = false; /* 合成后端此时确认旧操作已完成并排空。 */
    return meter_calibration_reply(&app->calibration, &app->core.snapshot.profile, prerequisites(app),
                                   &app->authorization, now, &reply);
}
static void profile_measurement_and_presentation(void)
{
    bool accepted;
    product_app_t app;
    setup(&app);
    product_view_t unknown = present(&app, 1);
    assert(!unknown.capture_supported && unknown.measurement_state == METER_VALUE_UNKNOWN);
    accepted = acquire(&app, 0, 12, 0);
    assert(!accepted);
    replace_profile(&app, 0xA1, 0, 1);
    accepted = acquire(&app, 1, 12, 2);
    assert(accepted);
    assert(meter_authorization_grant(&app.authorization, 1, 2, 100));
    begin(&app, 1, 3);
    assert(present(&app, 1).outcome == METER_CALIBRATION_PREREQUISITE);
    accepted = dispatch(&app, 3);
    assert(!accepted);
    assert(meter_calibration_acknowledge(&app.calibration, 1, 1));
    replace_profile(&app, 0xA1, 0x40, 4);
    product_view_t changed = present(&app, 2);
    assert(changed.profile_generation == 2 && changed.capture_supported);
    assert(changed.measurement_state == METER_VALUE_UNKNOWN);
    accepted = acquire(&app, 1, 90, 5);
    assert(!accepted); /* 迟到测量不得换上新代数。 */
    accepted = acquire(&app, 2, 0, 5);
    assert(accepted);
    product_view_t valid_zero = present(&app, 2);
    assert(valid_zero.measurement == 0 && valid_zero.measurement_state == METER_VALUE_VALID);
    replace_profile(&app, 0xA1, 0x41, 6); /* 未映射功能位不改变规范化语义。 */
    assert(present(&app, 2).revision == valid_zero.revision);
    meter_core_tick(&app.core, 55);
    product_view_t stale = present(&app, 2);
    assert(stale.measurement_state == METER_VALUE_STALE && stale.revision > valid_zero.revision);
    begin(&app, 2, 55);
    assert(present(&app, 2).outcome == METER_CALIBRATION_SOURCE_UNAVAILABLE);
    accepted = dispatch(&app, 55);
    assert(!accepted);
    assert(valid_zero.measurement_state == METER_VALUE_VALID);
    replace_profile(&app, 0xFF, 0x40, 56);
    assert(!present(&app, 2).capture_supported);
    assert(present(&app, 2).measurement_state == METER_VALUE_UNKNOWN);
    assert(!meter_profile_matches(&app.core.snapshot.profile, valid_zero.profile_generation));
}
static void profile_change_preserves_uncertainty_and_backend_ownership(void)
{
    bool accepted;
    product_app_t app;
    setup(&app);
    replace_profile(&app, 0xA1, 0x40, 0);
    accepted = acquire(&app, 1, 12, 1);
    assert(accepted);
    assert(meter_authorization_grant(&app.authorization, 1, 1, 100));
    begin(&app, 10, 2);
    accepted = dispatch(&app, 3);
    assert(accepted);
    assert(app.backend_work.key.owner == 2 && app.backend_work.key.id == 9);
    assert(app.backend_work.value == 12);
    replace_profile(&app, 0xFF, 0, 4);
    product_view_t historical = present(&app, 10);
    assert(historical.has_result && !historical.result_current && historical.effect_unknown);
    assert(historical.outcome == METER_CALIBRATION_PROFILE_CHANGED);
    assert(historical.result_generation == 1 && historical.result_token == 10);
    assert(app.backend_busy && !meter_authorization_allows(&app.authorization, 1, 4));
    assert(!meter_calibration_acknowledge(&app.calibration, 2, 10));
    assert(meter_calibration_acknowledge(&app.calibration, 1, 10));
    replace_profile(&app, 0xA1, 0x40, 5);
    accepted = acquire(&app, 1, 99, 6);
    assert(!accepted);
    accepted = acquire(&app, 3, 24, 6);
    assert(accepted);
    assert(meter_authorization_grant(&app.authorization, 1, 6, 100));
    begin(&app, 11, 7);
    accepted = dispatch(&app, 8);
    assert(!accepted);
    accepted = complete(&app, 12, 9);
    assert(!accepted); /* 旧写入仅释放旧后端，不能完成新流程。 */
    assert(present(&app, 11).pending && !present(&app, 11).has_result);
    accepted = dispatch(&app, 10);
    assert(accepted);
    assert(app.backend_work.operation == METER_PARAMETER_WRITE && app.backend_work.value == 24);
    accepted = acquire(&app, 3, 80, 11);
    assert(accepted); /* 持续测量不覆盖用户已捕获的值。 */
    accepted = complete(&app, 24, 12);
    assert(accepted);
    accepted = dispatch(&app, 13);
    assert(accepted);
    assert(app.backend_work.operation == METER_PARAMETER_READ);
    accepted = complete(&app, 24.05f, 14);
    assert(accepted);
    product_view_t verified = present(&app, 11);
    assert(verified.has_result && verified.result_current && !verified.effect_unknown);
    assert(verified.outcome == METER_CALIBRATION_OK && verified.measurement == 80);
    assert(!present(&app, 10).result_current);
    assert(historical.effect_unknown && historical.result_generation == 1);
}
static void expired_authorization_is_classified_without_health_policy(void)
{
    bool accepted;
    product_app_t app;
    setup(&app);
    replace_profile(&app, 0xA1, 0x40, 0);
    accepted = acquire(&app, 1, 12, 1);
    assert(accepted);
    assert(meter_authorization_grant(&app.authorization, 1, 1, 5));
    begin(&app, 20, 2);
    accepted = dispatch(&app, 3);
    assert(accepted);
    accepted = dispatch(&app, 6);
    assert(!accepted);
    product_view_t expired = present(&app, 20);
    assert(expired.has_result && expired.effect_unknown && expired.result_current);
    assert(expired.outcome == METER_CALIBRATION_TRANSACTION_FAILED);
    meter_calibration_view_t details;
    meter_calibration_present(&app.calibration, &details);
    assert(details.write_result.outcome == METER_PARAMETER_PERMISSION_LOST);
    assert(meter_parameter_classify(details.write_result.outcome) == METER_EVENT_EXPECTED_SUPPRESSION);
    assert(app.backend_busy);
    accepted = complete(&app, 12, 7);
    assert(!accepted);
}
int main(void)
{
    profile_measurement_and_presentation();
    profile_change_preserves_uncertainty_and_backend_ownership();
    expired_authorization_is_classified_without_health_policy();
    return 0;
}
