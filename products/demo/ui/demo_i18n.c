#include "ui/demo_i18n.h"
#include <lvgl.h>
#include <stddef.h>
LV_FONT_DECLARE(meter_demo_cjk_14);
LV_FONT_DECLARE(meter_demo_cjk_20);
static const char *const languages[] = {METER_LANGUAGE_CODE_EN, METER_LANGUAGE_CODE_ZH, NULL};
static const char *const tags[] = {
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
    "ACTIVE",
    "CLEAR",
    "SEAT",
    "BRAKE",
    "NEUTRAL",
    "CHARGE",
    "WARNING",
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
static const char *const translations[] = {
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
    "ACTIVE", "活动",
    "CLEAR ", "正常 ",
    "Seat", "座椅",
    "Brake", "制动",
    "Neutral", "空挡",
    "Charge", "充电",
    "Warning", "告警",
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
_Static_assert(sizeof(tags) / sizeof(tags[0]) == DEMO_TXT_COUNT + 1, "demo tag count");
_Static_assert(sizeof(translations) / sizeof(translations[0]) == DEMO_TXT_COUNT * 2,
               "demo translation count");
#define DEMO_MONITOR_COUNT (DEMO_TXT_FAULT_0 - DEMO_TXT_MONITOR_0)
#define DEMO_FAULT_COUNT (DEMO_TXT_COUNT - DEMO_TXT_FAULT_0)
/* English already matches the runtime fallback, so only Chinese needs the checked-in subset. */
const lv_font_t *demo_font_resolve(meter_language_t language, meter_font_role_t role)
{
    if (language != METER_LANGUAGE_ZH)
        return NULL;
    return role == METER_FONT_VALUE ? &meter_demo_cjk_20 : &meter_demo_cjk_14;
}
bool demo_i18n_init(void)
{
    meter_font_provider_set(demo_font_resolve);
    return lv_translation_add_static(languages, tags, translations) != NULL;
}
const char *demo_i18n_tag(demo_text_id_t id)
{
    return (unsigned)id < DEMO_TXT_COUNT ? tags[id] : "";
}
const char *demo_i18n_text(demo_text_id_t id)
{
    return meter_i18n_tr(demo_i18n_tag(id));
}
void demo_i18n_bind_label(lv_obj_t *label, demo_text_id_t id)
{
    meter_i18n_bind_label(label, demo_i18n_tag(id));
}
const char *demo_i18n_monitor_label(size_t i)
{
    return i < DEMO_MONITOR_COUNT ? demo_i18n_text((demo_text_id_t)(DEMO_TXT_MONITOR_0 + i)) : "";
}
const char *demo_i18n_fault_description(size_t i)
{
    return i < DEMO_FAULT_COUNT ? demo_i18n_text((demo_text_id_t)(DEMO_TXT_FAULT_0 + i)) : "";
}
