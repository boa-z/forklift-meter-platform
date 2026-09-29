#include "services/settings_app.h"
#include "application/presentation.h"
#include "core/meter_core.h"
#include "core/meter_settings.h"
#include "product/demo_storage.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static demo_presentation_t present(meter_core_t *core)
{
    demo_settings_publish(&core->snapshot);
    demo_presentation_t view;
    demo_presentation_build(&core->snapshot, &view);
    return view;
}
static meter_core_t core;
static bool intent(demo_settings_intent_t id, float value, uint32_t now)
{
    meter_action_t action = demo_settings_intent(id, value);
    return demo_settings_action(&core.snapshot, &action, now);
}
int main(void)
{
    demo_domain_store_t store;
    meter_core_storage_t storage = demo_domain_bind(&store);
    bool ok = meter_core_init(&core, &meter_demo_catalog, &storage);
    assert(ok);
    float local[DEMO_PARAMETER_SLOTS];
    memcpy(local, core.snapshot.parameters, sizeof(local));
    demo_settings_reset(1);
    demo_presentation_t view = present(&core);
    assert(!view.admin_authorized && !view.user_authorized);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 42, 1);
    assert(!ok);
    ok = intent(DEMO_INTENT_USER_LOGIN, 1234, 2);
    assert(ok);
    view = present(&core);
    assert(view.user_authorized && !view.admin_authorized);
    ok = intent(DEMO_INTENT_ADMIN_FIRST, 1, 3);
    assert(!ok);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, 10);
    assert(core.snapshot.can_rate == METER_CAN_RATE_500K);
    assert(ok);
    view = present(&core);
    assert(view.admin_authorized);
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
    {
        meter_action_t action = demo_admin_intent(i, 1);
        ok = demo_settings_action(&core.snapshot, &action, 11);
        assert(ok);
    }
    view = present(&core);
    for (unsigned i = 0; i < DEMO_ADMIN_COUNT; ++i)
        assert(view.admin_values[i] == 1);
    assert(core.snapshot.can_rate == METER_CAN_RATE_250K);
    uint32_t saved_revision = core.snapshot.revision;
    assert(intent(DEMO_INTENT_ADMIN_FIRST, 1, 11));
    assert(core.snapshot.revision == saved_revision);
    assert(!intent(DEMO_INTENT_ADMIN_FIRST, 3, 11));
    assert(!intent(DEMO_INTENT_ADMIN_FIRST, -1, 11));
    assert(!intent(DEMO_INTENT_ADMIN_FIRST, 0.5f, 11));
    assert(!intent(DEMO_INTENT_ADMIN_FIRST, NAN, 11));
    assert(core.snapshot.can_rate == METER_CAN_RATE_250K);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 51, 12);
    assert(!ok);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, NAN, 12);
    assert(!ok);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 42, 12);
    assert(ok);
    view = present(&core);
    assert(view.parameter_feedback == DEMO_FEEDBACK_QUEUED && view.parameters[0].value == 25);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 41, 13);
    assert(!ok);
    demo_settings_run(14, NULL);
    view = present(&core);
    assert(view.parameter_feedback == DEMO_FEEDBACK_APPLIED && view.parameters[0].value == 42);
    assert(view.parameters[2].value == 4);
    /* 同编号不同 owner 的参数独立；不会落入本机持久化数组。 */
    meter_action_t other = demo_remote_intent(2, 6);
    ok = demo_settings_action(&core.snapshot, &other, 15);
    assert(ok);
    demo_settings_run(16, NULL);
    view = present(&core);
    assert(view.parameters[0].value == 42 && view.parameters[2].value == 6);
    assert(memcmp(local, core.snapshot.parameters, sizeof(local)) == 0);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 40, 17);
    assert(ok);
    ok = intent(DEMO_INTENT_LOGOUT, 0, 18);
    assert(ok);
    demo_settings_run(19, NULL);
    view = present(&core);
    assert(!view.admin_authorized && view.parameters[0].value == 42);
    assert(view.parameter_feedback == DEMO_FEEDBACK_DENIED);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, 20);
    assert(ok);
    ok = intent(DEMO_INTENT_REMOTE_FIRST, 40, 21);
    assert(ok);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, 22);
    assert(ok);
    demo_settings_run(23, NULL);
    view = present(&core);
    assert(view.parameters[0].value == 42 && view.parameter_feedback == DEMO_FEEDBACK_DENIED);
    demo_settings_run(60022, NULL);
    view = present(&core);
    assert(!view.admin_authorized && view.auth_feedback == DEMO_FEEDBACK_EXPIRED);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, UINT32_MAX - 30u);
    assert(ok);
    demo_settings_run(100, NULL);
    view = present(&core);
    assert(view.admin_authorized);
    demo_settings_run(59969, NULL);
    view = present(&core);
    assert(!view.admin_authorized);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, 60000);
    assert(ok);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 1234, 60001);
    assert(!ok);
    view = present(&core);
    assert(!view.user_authorized && !view.admin_authorized);
    ok = intent(DEMO_INTENT_ADMIN_LOGIN, 5312, 60002);
    assert(ok);
    demo_settings_reset(2);
    view = present(&core);
    assert(!view.admin_authorized);
    assert(memcmp(local, core.snapshot.parameters, sizeof(local)) == 0);
    /* 模拟重启：只恢复保留设置，不恢复管理员权限或远端事务。 */
    uint8_t retained[256];
    assert(meter_settings_encode(&core, retained, sizeof(retained)));
    assert(meter_core_init(&core, &meter_demo_catalog, &storage));
    assert(core.snapshot.can_rate == METER_CAN_RATE_500K);
    assert(meter_settings_decode(&core, retained, meter_settings_size(&core)));
    view = present(&core);
    assert(view.admin_values[0] == 1 && !view.admin_authorized);
    assert(!intent(DEMO_INTENT_ADMIN_FIRST, 2, 60003));
    assert(core.snapshot.can_rate == METER_CAN_RATE_250K);
    return 0;
}
