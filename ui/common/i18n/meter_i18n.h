#ifndef METER_I18N_H
#define METER_I18N_H
#include "contracts/meter_domain.h"
typedef struct _lv_obj_t lv_obj_t;
typedef enum
{
    METER_TXT_FIELD,
    METER_TXT_REFERENCE,
    METER_TXT_WAITING,
    METER_TXT_CONNECTED,
    METER_TXT_OFFLINE,
    METER_TXT_STALE,
    METER_TXT_DASHBOARD,
    METER_TXT_MONITOR,
    METER_TXT_FAULTS,
    METER_TXT_SETTINGS,
    METER_TXT_TRACTION,
    METER_TXT_ENERGY,
    METER_TXT_STEERING,
    METER_TXT_LIFT_HEIGHT,
    METER_TXT_LOAD,
    METER_TXT_LIVE_TELEMETRY,
    METER_TXT_TELEMETRY_SUBTITLE,
    METER_TXT_ADVISORIES,
    METER_TXT_FAULT_SUBTITLE,
    METER_TXT_LOCAL_PREFERENCES,
    METER_TXT_SETTINGS_SUBTITLE,
    METER_TXT_SPEED_UNITS,
    METER_TXT_BRIGHTNESS,
    METER_TXT_SPEED_LIMIT,
    METER_TXT_LIMITS_NOTE,
    METER_TXT_SETTINGS_OK,
    METER_TXT_SETTINGS_ERROR,
    METER_TXT_METRIC,
    METER_TXT_IMPERIAL,
    METER_TXT_LANGUAGE,
    METER_TXT_ENGLISH,
    METER_TXT_CHINESE,
    METER_TXT_LIVE,
    METER_TXT_STALE_STATE,
    METER_TXT_NO_DATA,
    METER_TXT_ERROR_STATE,
    METER_TXT_ACTIVE,
    METER_TXT_CLEAR,
    METER_TXT_SEAT,
    METER_TXT_BRAKE,
    METER_TXT_NEUTRAL,
    METER_TXT_CHARGE,
    METER_TXT_WARNING,
    METER_TXT_ON,
    METER_TXT_OFF,
    METER_TXT_MONITOR_0,
    METER_TXT_MONITOR_1,
    METER_TXT_MONITOR_2,
    METER_TXT_MONITOR_3,
    METER_TXT_MONITOR_4,
    METER_TXT_MONITOR_5,
    METER_TXT_MONITOR_6,
    METER_TXT_MONITOR_7,
    METER_TXT_MONITOR_8,
    METER_TXT_MONITOR_9,
    METER_TXT_MONITOR_10,
    METER_TXT_MONITOR_11,
    METER_TXT_MONITOR_12,
    METER_TXT_MONITOR_13,
    METER_TXT_FAULT_0,
    METER_TXT_FAULT_1,
    METER_TXT_FAULT_2,
    METER_TXT_FAULT_3,
    METER_TXT_FAULT_4,
    METER_TXT_FAULT_5,
    METER_TXT_FAULT_6,
    METER_TXT_FAULT_7,
    METER_TXT_FAULT_8,
    METER_TXT_FAULT_9,
    METER_TXT_COUNT
} meter_text_id_t;
bool meter_i18n_init(void);
const char *meter_i18n_tag(meter_text_id_t id);
void meter_i18n_bind_label(lv_obj_t *label, meter_text_id_t id);
const char *meter_i18n_text(meter_text_id_t id);
const char *meter_i18n_monitor_label(size_t index);
const char *meter_i18n_fault_description(size_t index);
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language);
const char *meter_i18n_state(meter_value_state_t state);
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language);
#endif
