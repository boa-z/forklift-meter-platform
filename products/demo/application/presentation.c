#include "application/presentation.h"
#include "meter_build_identity.h"
#ifdef METER_ENABLE_CAN_UPDATE
#include "meter_update_build.h"
#endif
static demo_readout_t readout(const meter_snapshot_t *snapshot, meter_signal_id_t id)
{
    meter_value_t value = meter_snapshot_read(snapshot, id);
    return (demo_readout_t){value.value, value.state};
}
void demo_presentation_build(const meter_snapshot_t *s, demo_presentation_t *out)
{
    demo_presentation_t view = {0};
#ifdef METER_ENABLE_CAN_UPDATE
    view.firmware_version = METER_UPDATE_FIRMWARE_VERSION;
#endif
    view.framework_revision = METER_BUILD_PLATFORM;
    view.revision = s->revision;
    view.profile = s->profile;
    view.language = s->language;
    view.imperial = s->imperial;
    view.brightness = s->brightness;
    view.limit_available = meter_snapshot_parameter(s, DEMO_PARAMETER_MAX_SPEED, &view.limit);
    view.speed = readout(s, METER_SPEED);
    view.steering = readout(s, METER_STEERING);
    view.soc = readout(s, METER_SOC);
    view.height = readout(s, METER_HEIGHT);
    view.load = readout(s, METER_LOAD);
    view.speed.value *= s->imperial ? 0.621371f : 1.0f;
    view.speed_maximum = s->imperial ? 32.0f : 50.0f;
    view.speed_unit = s->imperial ? "mph" : "km/h";
    const meter_signal_id_t flags[] = {METER_SEAT, METER_BRAKE, METER_NEUTRAL, METER_CHARGING, METER_WARNING};
    for (size_t i = 0; i < 5; ++i)
        view.status[i] = readout(s, flags[i]);
    bool stale = false;
    for (size_t i = 0; i < s->catalog->signal_count; ++i)
        if (s->signals[i].state == METER_VALUE_STALE)
            stale = true;
    view.link = !s->connected                             ? DEMO_LINK_OFFLINE
                : stale                                   ? DEMO_LINK_STALE
                : view.speed.state == METER_VALUE_UNKNOWN ? DEMO_LINK_WAITING
                                                          : DEMO_LINK_CONNECTED;
    for (size_t i = 0; i < DEMO_MONITOR_SLOTS; ++i)
    {
        const meter_monitor_def_t *definition = &meter_demo_catalog.monitors[i];
        view.monitors[i].reading = readout(s, definition->signal);
        view.monitors[i].unit = definition->unit;
    }
    const meter_signal_id_t remote[] = {DEMO_REMOTE_SPEED, DEMO_REMOTE_RAMP, DEMO_REMOTE_LIFT, DEMO_REMOTE_REGEN};
    for (size_t i = 0; i < DEMO_REMOTE_COUNT; ++i)
    {
        meter_value_t value = meter_snapshot_read(s, remote[i]);
        view.parameters[i] = (demo_parameter_view_t){value.value, value.state == METER_VALUE_VALID};
    }
    meter_value_t access = meter_snapshot_read(s, DEMO_ACCESS_ROLE);
    view.user_authorized = access.state == METER_VALUE_VALID && access.value >= 1;
    view.admin_authorized = access.state == METER_VALUE_VALID && access.value == 2;
    view.auth_feedback = (demo_feedback_t)meter_snapshot_read(s, DEMO_ACCESS_RESULT).value;
    view.parameter_feedback = (demo_feedback_t)meter_snapshot_read(s, DEMO_REMOTE_RESULT).value;
    const meter_signal_id_t admin[] = {DEMO_ADMIN_CAN, DEMO_ADMIN_HOURS, DEMO_ADMIN_SPEED, DEMO_ADMIN_MEMORY};
    for (size_t i = 0; i < DEMO_ADMIN_COUNT; ++i)
        view.admin_values[i] = (unsigned)meter_snapshot_read(s, admin[i]).value;
    for (size_t i = 0; i < DEMO_FAULT_SLOTS; ++i)
    {
        view.faults[i].code = meter_demo_catalog.faults[i].id;
        view.faults[i].active = meter_snapshot_fault_active(s, view.faults[i].code);
    }
    *out = view;
}
meter_action_t demo_speed_limit_intent(float value)
{
    return (meter_action_t){METER_ACTION_PARAMETER, DEMO_PARAMETER_MAX_SPEED, value};
}

meter_action_t demo_settings_intent(demo_settings_intent_t intent, float value)
{
    return (meter_action_t){METER_ACTION_PRODUCT, (uint16_t)intent, value};
}
meter_action_t demo_remote_intent(unsigned row, float value)
{
    return demo_settings_intent((demo_settings_intent_t)(DEMO_INTENT_REMOTE_FIRST + row), value);
}
meter_action_t demo_admin_intent(unsigned row, float value)
{
    return demo_settings_intent((demo_settings_intent_t)(DEMO_INTENT_ADMIN_FIRST + row), value);
}
