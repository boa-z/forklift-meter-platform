/* i18n 服务契约：框架不内置任何措辞，语言、翻译包与字体由 Product 注册，切换语言是唯一入口。 */
#include "ui/common/i18n/meter_i18n_runtime.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include "check.h"

/* Product 自己的键。第三种语言 de 只翻译了一部分。 */
static const char *const pack_languages[] = {METER_LANGUAGE_CODE_EN, METER_LANGUAGE_CODE_ZH, "de", NULL};
static const char *const pack_tags[] = {"demo.title", "demo.only_en", "demo.count.one", "demo.count.other", NULL};
static const char *const pack_texts[] = {
    "Title",        "标题",     "Titel",        /* demo.title */
    "English only", NULL,       NULL,           /* 其余语言没有这一行：回退到英文 */
    "{0} item",     "{0} 项",   "{0} Eintrag",  /* demo.count.one */
    "{0} items",    "{0} 项",   "{0} Einträge", /* demo.count.other */
};
static const meter_i18n_pack_t pack = {pack_languages, pack_tags, pack_texts};
static const meter_language_t LANGUAGE_DE = (meter_language_t)2;

static int language_flow(void)
{
    /* 刚初始化：默认语言表（en、zh-CN），选中英文；没有任何包时取词回退为标签本身。 */
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_EN && meter_i18n_language_count() == 2u);
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "demo.title"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_ZH);
    CHECK(meter_i18n_set_language(METER_LANGUAGE_EN));

    /* 同一个包再注册一次是空操作。 */
    CHECK(meter_i18n_register_pack(&pack));
    CHECK(meter_i18n_register_pack(&pack));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "Title"));
    CHECK(meter_i18n_set_language(METER_LANGUAGE_ZH));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "标题"));
    /* 显式语言取词不依赖当前选择。 */
    CHECK(!strcmp(meter_i18n_tr_in(METER_LANGUAGE_EN, "demo.title"), "Title"));

    /* 第三种语言：先要注册，未注册的 ID 拒绝且不改当前语言。 */
    CHECK(!meter_i18n_set_language(LANGUAGE_DE));
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_ZH);
    /* 语言表属于 Product：编号即表内位置，打乱编号或重复语言码的表整张拒绝，原表保持不变。 */
    static const meter_i18n_language_t shuffled[] = {
        {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, NULL, NULL, METER_PLURAL_OTHER_ONLY},
        {METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_ONE_OTHER}};
    static const meter_i18n_language_t twice[] = {
        {METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_ONE_OTHER},
        {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_OTHER_ONLY}};
    CHECK(!meter_i18n_register_languages(shuffled, 2u) && !meter_i18n_register_languages(twice, 2u));
    CHECK(meter_i18n_language_count() == 2u);
    static const meter_i18n_language_t three[] = {
        {METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_ONE_OTHER},
        {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, NULL, NULL, METER_PLURAL_OTHER_ONLY},
        {LANGUAGE_DE, "de", NULL, NULL, METER_PLURAL_ONE_OTHER}};
    CHECK(meter_i18n_register_languages(three, 3u) && meter_i18n_register_languages(three, 3u));
    CHECK(meter_i18n_language_count() == 3u);
    CHECK(meter_i18n_set_language(LANGUAGE_DE));
    CHECK(meter_i18n_selected_language() == LANGUAGE_DE);
    CHECK(!strcmp(meter_i18n_language_code(LANGUAGE_DE), "de"));
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "Titel"));
    /* 单元格为空的行回退到英文列；包里根本没有的标签回退为标签本身。 */
    CHECK(!strcmp(meter_i18n_tr("demo.only_en"), "English only"));
    CHECK(!strcmp(meter_i18n_tr("demo.missing"), "demo.missing"));
    /* find 只看当前语言：有译文给译文，否则 NULL，不回退英文也不回退标签。 */
    CHECK(!strcmp(meter_i18n_find("demo.title"), "Titel"));
    CHECK(meter_i18n_find("demo.only_en") == NULL && meter_i18n_find("demo.missing") == NULL);
    CHECK(meter_i18n_find("") == NULL && meter_i18n_find(NULL) == NULL);
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
    const meter_i18n_arg_t minimum = meter_i18n_arg_s(INT32_MIN);
    CHECK(meter_i18n_format(text, sizeof(text), "{0}", &minimum, 1u) == 11u &&
          !strcmp(text, "-2147483648"));
    CHECK(meter_i18n_format(text, 6u, "{0}", &minimum, 1u) == 5u && !strcmp(text, "-2147"));
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
    /* 注册表借用语言表，表必须活得比 LVGL 久。 */
    static const meter_i18n_language_t faces[] = {
        {METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_ONE_OTHER},
        {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, &lv_font_montserrat_20, &lv_font_montserrat_20,
         METER_PLURAL_OTHER_ONLY}};
    CHECK(meter_i18n_register_languages(faces, 2u));
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
    CHECK(meter_i18n_selected_language() == METER_LANGUAGE_EN && meter_i18n_language_count() == 2u);
    CHECK(!strcmp(meter_i18n_tr("demo.title"), "demo.title"));
    lv_deinit();
    puts("Framework i18n: Product language tables and packs, runtime switch and face "
         "binding PASS");
    return 0;
}
