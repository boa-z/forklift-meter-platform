/* i18n 服务契约：框架不内置任何措辞，语言、翻译包与字体由 Product 注册，切换语言是唯一入口。 */
#include "ui/common/i18n/hmi_i18n_runtime.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include "check.h"

/* Product 自己的 lv_i18n 语言包。第一项是回退语言（英文）；第三种语言 de 只翻译了一部分。 */
static const lv_i18n_phrase_t en_singulars[] = {
    {"demo.title", "Title"}, {"demo.only_en", "English only"}, {NULL, NULL}};
static const lv_i18n_phrase_t en_one[] = {{"demo.count", "{0} item"}, {NULL, NULL}};
static const lv_i18n_phrase_t en_other[] = {{"demo.count", "{0} items"}, {NULL, NULL}};
/* zh 与 de 没有 demo.only_en：回退到英文。 */
static const lv_i18n_phrase_t zh_singulars[] = {{"demo.title", "标题"}, {NULL, NULL}};
static const lv_i18n_phrase_t zh_other[] = {{"demo.count", "{0} 项"}, {NULL, NULL}};
static const lv_i18n_phrase_t de_singulars[] = {{"demo.title", "Titel"}, {NULL, NULL}};
static const lv_i18n_phrase_t de_one[] = {{"demo.count", "{0} Eintrag"}, {NULL, NULL}};
static const lv_i18n_phrase_t de_other[] = {{"demo.count", "{0} Einträge"}, {NULL, NULL}};
static uint8_t one_other(int32_t n)
{
    return n == 1 ? LV_I18N_PLURAL_TYPE_ONE : LV_I18N_PLURAL_TYPE_OTHER;
}
static uint8_t other_only(int32_t n)
{
    (void)n;
    return LV_I18N_PLURAL_TYPE_OTHER;
}
static const lv_i18n_lang_t en_lang = {
    .locale_name = HMI_LANGUAGE_CODE_EN,
    .singulars = en_singulars,
    .plurals = {[LV_I18N_PLURAL_TYPE_ONE] = en_one, [LV_I18N_PLURAL_TYPE_OTHER] = en_other},
    .locale_plural_fn = one_other};
static const lv_i18n_lang_t zh_lang = {.locale_name = HMI_LANGUAGE_CODE_ZH,
                                       .singulars = zh_singulars,
                                       .plurals = {[LV_I18N_PLURAL_TYPE_OTHER] = zh_other},
                                       .locale_plural_fn = other_only};
static const lv_i18n_lang_t de_lang = {
    .locale_name = "de",
    .singulars = de_singulars,
    .plurals = {[LV_I18N_PLURAL_TYPE_ONE] = de_one, [LV_I18N_PLURAL_TYPE_OTHER] = de_other},
    .locale_plural_fn = one_other};
static const lv_i18n_language_pack_t pack[] = {&en_lang, &zh_lang, &de_lang, NULL};
static const lv_i18n_language_pack_t other_pack[] = {&zh_lang, NULL};
static const hmi_language_t LANGUAGE_DE = (hmi_language_t)2;

static int language_flow(void)
{
    /* 刚初始化：默认语言表（en、zh-CN），选中英文；没有任何包时取词回退为标签本身。 */
    CHECK(hmi_i18n_selected_language() == HMI_LANGUAGE_EN && hmi_i18n_language_count() == 2u);
    CHECK(!strcmp(hmi_i18n_tr("demo.title"), "demo.title"));
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_ZH));
    CHECK(hmi_i18n_selected_language() == HMI_LANGUAGE_ZH);
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));

    /* 同一个包再注册一次是空操作；一个 Product 只有一个包，换一个包被拒绝。 */
    CHECK(hmi_i18n_register_pack(pack));
    CHECK(hmi_i18n_register_pack(pack));
    CHECK(!hmi_i18n_register_pack(other_pack) && !hmi_i18n_register_pack(NULL));
    CHECK(!strcmp(hmi_i18n_tr("demo.title"), "Title"));
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_ZH));
    CHECK(!strcmp(hmi_i18n_tr("demo.title"), "标题"));
    /* 显式语言取词不依赖当前选择。 */
    CHECK(!strcmp(hmi_i18n_tr_in(HMI_LANGUAGE_EN, "demo.title"), "Title"));

    /* 第三种语言：先要注册，未注册的 ID 拒绝且不改当前语言。 */
    CHECK(!hmi_i18n_set_language(LANGUAGE_DE));
    CHECK(hmi_i18n_selected_language() == HMI_LANGUAGE_ZH);
    /* 语言表属于 Product：编号即表内位置，打乱编号或重复语言码的表整张拒绝，原表保持不变。 */
    static const hmi_i18n_language_t shuffled[] = {
        {HMI_LANGUAGE_ZH, HMI_LANGUAGE_CODE_ZH, NULL, NULL},
        {HMI_LANGUAGE_EN, HMI_LANGUAGE_CODE_EN, NULL, NULL}};
    static const hmi_i18n_language_t twice[] = {
        {HMI_LANGUAGE_EN, HMI_LANGUAGE_CODE_EN, NULL, NULL},
        {HMI_LANGUAGE_ZH, HMI_LANGUAGE_CODE_EN, NULL, NULL}};
    CHECK(!hmi_i18n_register_languages(shuffled, 2u) && !hmi_i18n_register_languages(twice, 2u));
    CHECK(hmi_i18n_language_count() == 2u);
    static const hmi_i18n_language_t three[] = {
        {HMI_LANGUAGE_EN, HMI_LANGUAGE_CODE_EN, NULL, NULL},
        {HMI_LANGUAGE_ZH, HMI_LANGUAGE_CODE_ZH, NULL, NULL},
        {LANGUAGE_DE, "de", NULL, NULL}};
    CHECK(hmi_i18n_register_languages(three, 3u) && hmi_i18n_register_languages(three, 3u));
    CHECK(hmi_i18n_language_count() == 3u);
    CHECK(hmi_i18n_set_language(LANGUAGE_DE));
    CHECK(hmi_i18n_selected_language() == LANGUAGE_DE);
    CHECK(!strcmp(hmi_i18n_language_code(LANGUAGE_DE), "de"));
    CHECK(!strcmp(hmi_i18n_tr("demo.title"), "Titel"));
    /* 某种语言没有的行回退到包的第一项（英文）；包里根本没有的标签回退为标签本身。 */
    CHECK(!strcmp(hmi_i18n_tr("demo.only_en"), "English only"));
    CHECK(!strcmp(hmi_i18n_tr("demo.missing"), "demo.missing"));
    /* find 只看当前语言：有译文给译文，否则 NULL，不回退英文也不回退标签。 */
    CHECK(!strcmp(hmi_i18n_find("demo.title"), "Titel"));
    CHECK(hmi_i18n_find("demo.only_en") == NULL && hmi_i18n_find("demo.missing") == NULL);
    CHECK(hmi_i18n_find("") == NULL && hmi_i18n_find(NULL) == NULL);
    CHECK(!strcmp(hmi_i18n_tr(""), ""));
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));
    return 0;
}

/* 模板是数据：位置占位符、可重排、花括号转义、缺参数原样保留、截断仍以 NUL 结尾。 */
static int format_flow(void)
{
    char text[32];
    const hmi_i18n_arg_t args[] = {hmi_i18n_arg_u(3u), hmi_i18n_arg_str("kg"), hmi_i18n_arg_s(-45)};
    CHECK(hmi_i18n_format(text, sizeof(text), "At most {0} {1}, {2}", args, 3u) == 17u);
    CHECK(!strcmp(text, "At most 3 kg, -45"));
    CHECK(hmi_i18n_format(text, sizeof(text), "{1} comes before {0}", args, 3u) > 0u);
    CHECK(!strcmp(text, "kg comes before 3"));
    CHECK(hmi_i18n_format(text, sizeof(text), "{{0}} {3} {x} {", args, 3u) > 0u);
    CHECK(!strcmp(text, "{0} {3} {x} {"));
    CHECK(hmi_i18n_format(text, 6u, "At most {0}", args, 3u) == 5u);
    CHECK(!strcmp(text, "At mo"));
    CHECK(hmi_i18n_format(text, sizeof(text), "{0}", NULL, 0u) == 3u && !strcmp(text, "{0}"));
    CHECK(hmi_i18n_format(text, sizeof(text), "{0}",
                            &(hmi_i18n_arg_t){HMI_I18N_ARG_U32, {.u32 = 4294967295u}}, 1u) == 10u &&
          !strcmp(text, "4294967295"));
    const hmi_i18n_arg_t minimum = hmi_i18n_arg_s(INT32_MIN);
    CHECK(hmi_i18n_format(text, sizeof(text), "{0}", &minimum, 1u) == 11u &&
          !strcmp(text, "-2147483648"));
    CHECK(hmi_i18n_format(text, 6u, "{0}", &minimum, 1u) == 5u && !strcmp(text, "-2147"));
    CHECK(hmi_i18n_format(NULL, 0u, "x", NULL, 0u) == 0u);
    return 0;
}
/* 复数：按当前语言的 lv_i18n 规则选 one / other 表；规则只有 other 的语言恒取 other。 */
static int plural_flow(void)
{
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.count", 1u), "{0} item"));
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.count", 0u), "{0} items"));
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.count", 2u), "{0} items"));
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_ZH));
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.count", 1u), "{0} 项"));
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.count", 5u), "{0} 项"));
    /* 没有这个键：返回标签本身。 */
    CHECK(!strcmp(hmi_i18n_tr_plural("demo.nothing", 1u), "demo.nothing"));
    CHECK(!strcmp(hmi_i18n_tr_plural("", 1u), ""));
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));
    return 0;
}
static int binding_flow(void)
{
    static const lv_font_t *const designed = &lv_font_montserrat_16;
    const lv_font_t *substitute = &lv_font_montserrat_20;
    /* 注册表借用语言表，表必须活得比 LVGL 久。 */
    static const hmi_i18n_language_t faces[] = {
        {HMI_LANGUAGE_EN, HMI_LANGUAGE_CODE_EN, NULL, NULL},
        {HMI_LANGUAGE_ZH, HMI_LANGUAGE_CODE_ZH, &lv_font_montserrat_20, &lv_font_montserrat_20}};
    CHECK(hmi_i18n_register_languages(faces, 2u));
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_style_text_font(label, designed, 0);
    hmi_i18n_bind_text(label, "demo.title", HMI_FONT_LABEL);
    lv_obj_t *plain = lv_label_create(screen);
    lv_obj_set_style_text_font(plain, designed, 0);
    hmi_i18n_bind_font(plain, HMI_FONT_VALUE);

    /* 绑定时语言是英文：文字取英文列；英文没有替换字体，保持设计时的字体。 */
    CHECK(!strcmp(lv_label_get_text(label), "Title"));
    CHECK(lv_obj_get_style_text_font(label, 0) == designed);
    /* 一次 set_language：文字与字体一起刷新，不需要重建任何控件。 */
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_ZH));
    CHECK(!strcmp(lv_label_get_text(label), "标题"));
    CHECK(lv_obj_get_style_text_font(label, 0) == substitute);
    CHECK(lv_obj_get_style_text_font(plain, 0) == substitute);
    /* 回到没有替换字体的语言，恢复设计字体而不是停在上一种语言的字体上。 */
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));
    CHECK(!strcmp(lv_label_get_text(label), "Title"));
    CHECK(lv_obj_get_style_text_font(label, 0) == designed);
    CHECK(lv_obj_get_style_text_font(plain, 0) == designed);
    /* 之后的明文写入解除文字绑定，但字体绑定继续生效。 */
    lv_label_set_text(label, "static");
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_ZH));
    CHECK(!strcmp(lv_label_get_text(label), "static"));
    CHECK(lv_obj_get_style_text_font(label, 0) == substitute);
    CHECK(hmi_i18n_set_language(HMI_LANGUAGE_EN));
    lv_obj_delete(screen);
    return 0;
}

int main(void)
{
    lv_init();
    CHECK(lv_display_create(800, 480));
    CHECK(hmi_i18n_init());
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
    CHECK(hmi_i18n_init());
    CHECK(!hmi_i18n_set_language(LANGUAGE_DE));
    CHECK(hmi_i18n_selected_language() == HMI_LANGUAGE_EN && hmi_i18n_language_count() == 2u);
    CHECK(!strcmp(hmi_i18n_tr("demo.title"), "demo.title"));
    lv_deinit();
    puts("Framework i18n: Product language tables and packs, runtime switch and face "
         "binding PASS");
    return 0;
}
