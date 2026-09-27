#include "core/meter_core.h"
#include "platform/host/host_platform.h"
#include "ui/products/demo/demo_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static bool accept = true;
static bool send(void *context, const meter_action_t *action)
{
    return accept && meter_core_action(context, action);
}

static int check_glyphs(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_label_class))
    {
        const char *text = lv_label_get_text(obj);
        const unsigned char *p = (const unsigned char *)text;
        const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
        while (*p)
        {
            uint32_t cp = *p++;
            unsigned trailing = 0;
            if (cp >= 0xf0) { cp &= 7; trailing = 3; }
            else if (cp >= 0xe0) { cp &= 15; trailing = 2; }
            else if (cp >= 0xc0) { cp &= 31; trailing = 1; }
            for (unsigned i = 0; i < trailing; ++i)
            {
                CHECK((*p & 0xc0) == 0x80);
                cp = (cp << 6) | (*p++ & 0x3f);
            }
            if (cp < 32) continue;
            lv_font_glyph_dsc_t glyph = {0};
            if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0) || glyph.is_placeholder)
            {
                fprintf(stderr, "Missing glyph U+%04x in %s\n", (unsigned)cp, text);
                return 1;
            }
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        CHECK(check_glyphs(lv_obj_get_child(obj, (int32_t)i)) == 0);
    return 0;
}

int main(void)
{
    meter_core_t core;
    meter_value_t signal_slots[DEMO_SIGNAL_SLOTS];
    float parameter_slots[DEMO_PARAMETER_SLOTS];
    meter_fault_state_t fault_slots[DEMO_FAULT_SLOTS];
    meter_core_storage_t storage = {signal_slots, DEMO_SIGNAL_SLOTS, parameter_slots, DEMO_PARAMETER_SLOTS,
                                    fault_slots, DEMO_FAULT_SLOTS};
    CHECK(meter_core_init(&core, &meter_demo_catalog, &storage));
    lv_init();
    CHECK(meter_i18n_init());
    CHECK(demo_i18n_init());
    CHECK(!strcmp(demo_i18n_text((demo_text_id_t)-1), ""));
    CHECK(!strcmp(demo_i18n_text(DEMO_TXT_COUNT), ""));
    CHECK(meter_host_open(true));
    meter_ui_actions_t actions = {send, &core};
    demo_ui_t *ui = demo_ui_create(lv_screen_active(), &actions);
    CHECK(ui);
    size_t objects = meter_ui_object_count(ui->root);
    for (unsigned round = 0; round < 3; ++round)
    {
        meter_language_t language = round == 1 ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN;
        if (round)
            lv_obj_send_event(ui->language_button, LV_EVENT_CLICKED, NULL);
        CHECK(core.snapshot.language == language);
        if (language == METER_LANGUAGE_ZH)
            for (unsigned i = 0; i < METER_TXT_COUNT; ++i)
                /* 若结果仍等于标签本身，说明通用翻译包丢了这一条中文译文。 */
                CHECK(strcmp(meter_i18n_text((meter_text_id_t)i), meter_i18n_tag((meter_text_id_t)i)) != 0);
        for (unsigned state = METER_VALUE_UNKNOWN; state <= METER_VALUE_ERROR; ++state)
        {
            for (size_t i = 0; i < core.snapshot.catalog->signal_count; ++i)
            {
                core.snapshot.signals[i].state = (meter_value_state_t)state;
                core.snapshot.signals[i].value = 1;
            }
            for (size_t i = 0; i < core.snapshot.catalog->fault_count; ++i)
                core.snapshot.faults[i].active = state % 2 != 0;
            core.snapshot.connected = state != METER_VALUE_STALE;
            for (unsigned page = 0; page < DEMO_PAGE_COUNT; ++page)
            {
                demo_navigation_show(ui, page);
                demo_ui_present(ui, &core.snapshot, 16);
                lv_tick_inc(16);
                lv_timer_handler();
                CHECK(check_glyphs(ui->root) == 0);
                CHECK(meter_ui_object_count(ui->root) == objects);
                CHECK(!strcmp(lv_label_get_text(ui->nav_labels[0]), language == METER_LANGUAGE_ZH ? "仪表盘" : "Dashboard"));
                CHECK(!strcmp(lv_label_get_text(ui->monitor_labels[0]), language == METER_LANGUAGE_ZH ? "车速" : "Vehicle speed"));
                CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(ui->root, 0)), language == METER_LANGUAGE_ZH ? "现场仪表" : "FIELD"));
                CHECK(!strcmp(lv_translation_get_language(), meter_i18n_language_code(language)));
                for (unsigned i = 0; i < DEMO_TXT_COUNT; ++i)
                    CHECK(strlen(demo_i18n_text((demo_text_id_t)i)) > 0);
            }
        }
    }
    accept = false;
    lv_obj_send_event(ui->language_button, LV_EVENT_CLICKED, NULL);
    demo_ui_present(ui, &core.snapshot, 16);
    CHECK(core.snapshot.language == METER_LANGUAGE_EN && ui->action_failed);
    CHECK(!strcmp(lv_label_get_text(ui->setting_status), demo_i18n_text(DEMO_TXT_SETTINGS_ERROR)));
    CHECK(check_glyphs(ui->root) == 0);
    demo_ui_destroy(ui);
    meter_host_close();
    lv_deinit();
    puts("Bilingual pages, runtime switching, rejected actions and glyph coverage PASS");
    return 0;
}
