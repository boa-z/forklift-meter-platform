/* i18n 服务契约：框架只内置英文词汇，语言、翻译包与字体由 Product 注册，切换语言是唯一入口。 */
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do                                                                                                       \
    {                                                                                                        \
        if (!(condition))                                                                                    \
        {                                                                                                    \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                                          \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

/* 与框架同名的标签用来覆盖措辞；demo.* 是 Product 自己的键。第三种语言 de 只翻译了一半。 */
static const char *const pack_languages[] = {METER_LANGUAGE_CODE_EN, METER_LANGUAGE_CODE_ZH, "de", NULL};
static const char *const pack_tags[] = {
    "meter.validity.stale", "meter.value.error", "demo.title", "demo.only_en",
    "demo.count.one",       "demo.count.other",  NULL};
static const char *const pack_texts[] = {
    "STALE",        "过期",     "veraltet",     /* meter.validity.stale */
    "ERR {0}",      "错误 {0}", "Fehler {0}",   /* meter.value.error */
    "Title",        "标题",     "Titel",        /* demo.title */
    "English only", NULL,       NULL,           /* 其余语言没有这一行：回退到英文 */
    "{0} item",     "{0} 项",   "{0} Eintrag",  /* demo.count.one */
    "{0} items",    "{0} 项",   "{0} Einträge", /* demo.count.other */
};
static const meter_i18n_pack_t pack = {pack_languages, pack_tags, pack_texts};
static const meter_language_t LANGUAGE_DE = (meter_language_t)2;

static int language_flow(void)
{
    /* 刚初始化：只有英文，也只内置英文；中文没有注册包时回退到英文，绝不出现标签名。 */
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_EN);
    CHECK(!strcmp(meter_i18n_text(METER_TXT_VALIDITY_LIVE), "LIVE"));
    CHECK(!strcmp(meter_i18n_state(METER_VALUE_UNKNOWN), "NO DATA"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_ZH);
    CHECK(!strcmp(meter_i18n_text(METER_TXT_STATE_ON), "ON"));
    char text[48];
    meter_i18n_format_value(text, sizeof(text), 3.0f, METER_VALUE_ERROR, "kg", 1, METER_LANGUAGE_ZH);
    CHECK(!strcmp(text, "ERR kg"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));

    /* Product 的包后注册，覆盖框架词汇；同一个包再注册一次是空操作。 */
    CHECK(meter_i18n_register_pack(&pack));
    CHECK(meter_i18n_register_pack(&pack));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "Title"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "标题"));
    CHECK(!strcmp(meter_i18n_state(METER_VALUE_STALE), "过期"));
    meter_i18n_format_value(text, sizeof(text), 12.5f, METER_VALUE_STALE, "kg", 1, METER_LANGUAGE_ZH);
    CHECK(!strcmp(text, "12.5 kg / 过期"));
    meter_i18n_format_value(text, sizeof(text), NAN, METER_VALUE_VALID, "kg", 1, METER_LANGUAGE_ZH);
    CHECK(!strcmp(text, "错误 kg"));
    /* 未知读数只留占位符，与语言无关，也不因 NaN 被当成错误。 */
    meter_i18n_format_value(text, sizeof(text), NAN, METER_VALUE_UNKNOWN, "kg", 1, METER_LANGUAGE_ZH);
    CHECK(!strcmp(text, "-- kg"));
    /* 显式语言取词不依赖当前选择。 */
    CHECK(!strcmp(meter_i18n_tr_in(METER_LANGUAGE_EN, "demo.title"), "Title"));

    /* 第三种语言：先要注册，未注册的 ID 拒绝且不改当前语言。 */
    CHECK(!meter_i18n_set_language(LANGUAGE_DE));
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_ZH);
    const meter_i18n_language_t german = {LANGUAGE_DE, "de", NULL, NULL, METER_PLURAL_ONE_OTHER};
    CHECK(meter_i18n_register_language(&german));
    CHECK(meter_i18n_set_language(LANGUAGE_DE));
    CHECK(meter_i18n_selected_language() == LANGUAGE_DE);
    CHECK(!strcmp(meter_i18n_language_code(LANGUAGE_DE), "de"));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "Titel"));
    CHECK(!strcmp(meter_i18n_state(METER_VALUE_STALE), "veraltet"));
    /* 单元格为空的行回退到英文列；包里根本没有的标签回退为标签本身。 */
    CHECK(!strcmp(meter_i18n_tr("demo.only_en"), "English only"));
    CHECK(!strcmp(meter_i18n_tr("demo.missing"), "demo.missing"));
    /* 框架词汇没有 de 列，同样回退英文而不是显示标签。 */
    CHECK(!strcmp(meter_i18n_text(METER_TXT_STATE_OFF), "OFF"));
    CHECK(!strcmp(meter_i18n_tr(""), ""));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));
    return 0;
}

/* 模板是数据：位置占位符、可重排、花括号转义、缺参数原样保留、截断仍以 NUL 结尾。 */
static int format_flow(void)
{
    char text[32];
    const meter_i18n_arg_t args[] = {meter_i18n_arg_u(3u), meter_i18n_arg_str("kg"), meter_i18n_arg_s(-45)};
    CHECK(meter_i18n_format(text, sizeof(text), "At most {0} {1}, {2}", args, 3u) == 17u);
    CHECK(!strcmp(text, "At most 3 kg, -45"));
    CHECK(meter_i18n_format(text, sizeof(text), "{1} comes before {0}", args, 3u) > 0u);
    CHECK(!strcmp(text, "kg comes before 3"));
    CHECK(meter_i18n_format(text, sizeof(text), "{{0}} {3} {x} {", args, 3u) > 0u);
    CHECK(!strcmp(text, "{0} {3} {x} {"));
    CHECK(meter_i18n_format(text, 6u, "At most {0}", args, 3u) == 5u);
    CHECK(!strcmp(text, "At mo"));
    CHECK(meter_i18n_format(text, sizeof(text), "{0}", NULL, 0u) == 3u && !strcmp(text, "{0}"));
    CHECK(meter_i18n_format(text, sizeof(text), "{0}",
                            &(meter_i18n_arg_t){METER_I18N_ARG_U32, {.u32 = 4294967295u}}, 1u) == 10u &&
          !strcmp(text, "4294967295"));
    CHECK(meter_i18n_format(NULL, 0u, "x", NULL, 0u) == 0u);
    return 0;
}
/* 复数：按当前语言的规则选 .one / .other；规则只有 other 的语言恒取 other；缺 .one 时退到 .other。 */
static int plural_flow(void)
{
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));
    CHECK(!strcmp(meter_i18n_tr_plural("demo.count", 1u), "{0} item"));
    CHECK(!strcmp(meter_i18n_tr_plural("demo.count", 0u), "{0} items"));
    CHECK(!strcmp(meter_i18n_tr_plural("demo.count", 2u), "{0} items"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(!strcmp(meter_i18n_tr_plural("demo.count", 1u), "{0} 项"));
    CHECK(!strcmp(meter_i18n_tr_plural("demo.count", 5u), "{0} 项"));
    /* 没有这个键：返回基标签本身，而不是局部缓冲区。 */
    CHECK(!strcmp(meter_i18n_tr_plural("demo.nothing", 1u), "demo.nothing"));
    CHECK(!strcmp(meter_i18n_tr_plural("", 1u), ""));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));
    return 0;
}
static int binding_flow(void)
{
    static const lv_font_t *const designed = &lv_font_montserrat_16;
    const lv_font_t *substitute = &lv_font_montserrat_20;
    const meter_i18n_language_t chinese = {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, substitute, substitute,
                                           METER_PLURAL_OTHER_ONLY};
    CHECK(meter_i18n_register_language(&chinese));
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_style_text_font(label, designed, 0);
    meter_i18n_bind_text(label, "demo.title", METER_FONT_LABEL);
    lv_obj_t *plain = lv_label_create(screen);
    lv_obj_set_style_text_font(plain, designed, 0);
    meter_i18n_bind_font(plain, METER_FONT_VALUE);

    /* 绑定时语言是英文：文字取英文列；英文没有替换字体，保持设计时的字体。 */
    CHECK(!strcmp(lv_label_get_text(label), "Title"));
    CHECK(lv_obj_get_style_text_font(label, 0) == designed);
    /* 一次 set_language：文字与字体一起刷新，不需要重建任何控件。 */
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(!strcmp(lv_label_get_text(label), "标题"));
    CHECK(lv_obj_get_style_text_font(label, 0) == substitute);
    CHECK(lv_obj_get_style_text_font(plain, 0) == substitute);
    /* 回到没有替换字体的语言，恢复设计字体而不是停在上一种语言的字体上。 */
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));
    CHECK(!strcmp(lv_label_get_text(label), "Title"));
    CHECK(lv_obj_get_style_text_font(label, 0) == designed);
    CHECK(lv_obj_get_style_text_font(plain, 0) == designed);
    /* 之后的明文写入解除文字绑定，但字体绑定继续生效。 */
    lv_label_set_text(label, "static");
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(!strcmp(lv_label_get_text(label), "static"));
    CHECK(lv_obj_get_style_text_font(label, 0) == substitute);
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));
    lv_obj_delete(screen);
    return 0;
}

int main(void)
{
    lv_init();
    CHECK(lv_display_create(800, 480));
    CHECK(meter_i18n_init());
    CHECK(meter_i18n_init()); /* 同一次 LVGL 生命周期内重复初始化不丢注册 */
    if (language_flow())
        return 1;
    if (format_flow() || plural_flow())
        return 1;
    if (binding_flow())
        return 1;
    /* LVGL 重新初始化后注册表必须从头来过：上一轮的包和语言都不再有效。 */
    lv_deinit();
    lv_init();
    CHECK(lv_display_create(800, 480));
    CHECK(meter_i18n_init());
    CHECK(!meter_i18n_set_language(LANGUAGE_DE));
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_EN);
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "demo.title"));
    lv_deinit();
    puts("Framework i18n: English-only vocabulary, registered packs and languages, runtime switch and face "
         "binding PASS");
    return 0;
}
