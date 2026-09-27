#include "ui/common/i18n/meter_i18n.h"
#include "ui/common/formatter/meter_format.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char * const languages[] = {"en", "zh-CN", NULL};
static const char * const tags[] = {
    "FIELD",
    "REFERENCE",
    "WAITING",
    "CONNECTED",
    "OFFLINE",
    "STALE",
    "DASHBOARD",
    "MONITOR",
    "FAULTS",
    "SETTINGS",
    "TRACTION",
    "ENERGY",
    "STEERING",
    "LIFT_HEIGHT",
    "LOAD",
    "LIVE_TELEMETRY",
    "TELEMETRY_SUBTITLE",
    "ADVISORIES",
    "FAULT_SUBTITLE",
    "LOCAL_PREFERENCES",
    "SETTINGS_SUBTITLE",
    "SPEED_UNITS",
    "BRIGHTNESS",
    "SPEED_LIMIT",
    "LIMITS_NOTE",
    "SETTINGS_OK",
    "SETTINGS_ERROR",
    "METRIC",
    "IMPERIAL",
    "LANGUAGE",
    "ENGLISH",
    "CHINESE",
    "LIVE",
    "STALE_STATE",
    "NO_DATA",
    "ERROR_STATE",
    "ACTIVE",
    "CLEAR",
    "SEAT",
    "BRAKE",
    "NEUTRAL",
    "CHARGE",
    "WARNING",
    "ON",
    "OFF",
    "MONITOR_0",
    "MONITOR_1",
    "MONITOR_2",
    "MONITOR_3",
    "MONITOR_4",
    "MONITOR_5",
    "MONITOR_6",
    "MONITOR_7",
    "MONITOR_8",
    "MONITOR_9",
    "MONITOR_10",
    "MONITOR_11",
    "MONITOR_12",
    "MONITOR_13",
    "FAULT_0",
    "FAULT_1",
    "FAULT_2",
    "FAULT_3",
    "FAULT_4",
    "FAULT_5",
    "FAULT_6",
    "FAULT_7",
    "FAULT_8",
    "FAULT_9",
    NULL
};
static const char * const translations[] = {
    "FIELD", "现场仪表",
    "FORKLIFT / REFERENCE DEMO", "叉车 / 参考演示",
    "WAITING FOR TELEMETRY", "等待遥测数据",
    "SYNTHETIC / CONNECTED", "合成数据 / 已连接",
    "OFFLINE / VALUES RETAINED", "离线 / 保留数值",
    "STALE TELEMETRY", "遥测数据过期",
    "Dashboard", "仪表盘",
    "Monitor", "监控",
    "Faults", "故障",
    "Settings", "设置",
    "01 / TRACTION", "01 / 行驶",
    "02 / ENERGY", "02 / 能源",
    "03 / STEERING", "03 / 转向",
    "LIFT HEIGHT", "起升高度",
    "LOAD", "载荷",
    "LIVE TELEMETRY", "实时遥测",
    "Synthetic signals / domain values / validity retained", "合成信号 / 领域数值 / 保留有效性",
    "ADVISORIES", "告警提示",
    "Fictional examples only / no production diagnostic codes", "仅为虚构示例 / 不代表生产诊断码",
    "LOCAL PREFERENCES", "本地偏好",
    "Demo settings only / vehicle control is disabled", "仅为演示设置 / 车辆控制已禁用",
    "Speed units", "速度单位",
    "Display brightness", "显示亮度",
    "Demo speed limit / km/h", "演示速度上限 / km/h",
    "Limits are stored examples; synthetic playback is independent.", "上限仅为存储示例；与合成回放相互独立。",
    "English or Chinese / local settings / no maintenance authorization", "中文或英文 / 本地设置 / 无维护授权",
    "Settings request failed; retry after storage is available.", "设置请求失败；存储可用后请重试。",
    "Metric / km/h", "公制 / km/h",
    "Imperial / mph", "英制 / mph",
    "Language", "语言",
    "English", "英语",
    "中文", "中文",
    "LIVE", "实时",
    "STALE", "过期",
    "NO DATA", "无数据",
    "SENSOR ERROR", "传感器错误",
    "ACTIVE", "活动",
    "CLEAR ", "正常 ",
    "Seat", "座椅",
    "Brake", "制动",
    "Neutral", "空挡",
    "Charge", "充电",
    "Warning", "告警",
    "ON", "开",
    "OFF", "关",
    "Vehicle speed", "车速",
    "Battery charge", "电池电量",
    "Lift height", "起升高度",
    "Load weight", "载荷重量",
    "Steering angle", "转向角度",
    "Work hours", "工作小时",
    "Battery voltage", "电池电压",
    "Motor temperature", "电机温度",
    "Controller temperature", "控制器温度",
    "Seat occupied", "座椅占用",
    "Parking brake", "驻车制动",
    "Neutral", "空挡",
    "Charging", "充电中",
    "Generic warning", "一般告警",
    "Battery charge below demo threshold", "电池电量低于演示阈值",
    "Motor temperature advisory", "电机温度告警",
    "Battery voltage advisory", "电池电压告警",
    "Synthetic sensor reports an error", "合成传感器报告错误",
    "Telemetry unavailable or stale", "遥测不可用或已过期",
    "Load above demo advisory", "载荷超过演示阈值",
    "Lift above demo advisory", "起升高度超过演示阈值",
    "Controller temperature advisory", "控制器温度告警",
    "Injected demonstration warning", "注入的演示告警",
    "Steering angle advisory", "转向角度告警",
};
_Static_assert(sizeof(tags) / sizeof(tags[0]) == METER_TXT_COUNT + 1, "translation tag count");
_Static_assert(sizeof(translations) / sizeof(translations[0]) == METER_TXT_COUNT * 2, "translation value count");
bool meter_i18n_init(void)
{
    if (!lv_translation_add_static(languages, tags, translations))
        return false;
    lv_translation_set_language("en");
    return true;
}
const char *meter_i18n_tag(meter_text_id_t id)
{
    return (unsigned)id < METER_TXT_COUNT ? tags[id] : "";
}
const char *meter_i18n_text(meter_text_id_t id)
{
    return (unsigned)id < METER_TXT_COUNT ? lv_tr(tags[id]) : "";
}
const char *meter_i18n_monitor_label(size_t i)
{
    return i < 14 ? meter_i18n_text((meter_text_id_t)(METER_TXT_MONITOR_0 + i)) : "";
}
const char *meter_i18n_fault_description(size_t i)
{
    return i < 10 ? meter_i18n_text((meter_text_id_t)(METER_TXT_FAULT_0 + i)) : "";
}
const char *meter_i18n_state(meter_value_state_t state)
{
    meter_text_id_t id = state == METER_VALUE_UNKNOWN ? METER_TXT_NO_DATA
                         : state == METER_VALUE_STALE ? METER_TXT_STALE_STATE
                         : state == METER_VALUE_ERROR ? METER_TXT_ERROR_STATE : METER_TXT_LIVE;
    return meter_i18n_text(id);
}
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language)
{
    if (language != METER_LANGUAGE_ZH)
    {
        meter_format_value(out, size, value, state, unit, decimals);
        return;
    }
    if (state == METER_VALUE_ERROR || !isfinite(value))
        snprintf(out, size, "错误 %s", unit);
    else
    {
        meter_format_value(out, size, value, state == METER_VALUE_STALE ? METER_VALUE_VALID : state, unit, decimals);
        if (state == METER_VALUE_STALE && size)
        {
            size_t used = strlen(out);
            snprintf(out + used, size - used, " / %s", meter_i18n_text(METER_TXT_STALE_STATE));
        }
    }
}
