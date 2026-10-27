#ifndef METER_I18N_RUNTIME_H
#define METER_I18N_RUNTIME_H
#include "contracts/meter_domain.h"
#include <stddef.h>
#include <stdint.h>
typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_font_t lv_font_t;
/* Language codes name the columns of a translation pack. The framework ships no wording at all: a
   Product registers its own languages, packs and faces, so adding a language never edits the
   framework. */
#define METER_LANGUAGE_CODE_EN "en"
#define METER_LANGUAGE_CODE_ZH "zh-CN"
/* Size classes a label asks for. The language table decides which face answers them, so screens
   never name a language's font. */
typedef enum { METER_FONT_LABEL, METER_FONT_VALUE } meter_font_role_t;
/* Plural categories a language distinguishes. A plural text is the base tag plus ".one" / ".other"
   (see meter_i18n_tr_plural); more rules are added when a shipped language needs them. */
typedef enum
{
    METER_PLURAL_ONE_OTHER = 0, /* en, de, ...: a count of 1 is "one", everything else "other" */
    METER_PLURAL_OTHER_ONLY     /* zh, ja, ko: no grammatical number, always "other" */
} meter_plural_rule_t;
/* One selectable language. id is the persisted language byte (meter_language_t); code is the pack
   column. A NULL face means "keep the face the widget was designed with" for that role. */
typedef struct
{
    meter_language_t id;
    const char *code;
    const lv_font_t *label_font;
    const lv_font_t *value_font;
    meter_plural_rule_t plural;
} meter_i18n_language_t;
/* A static translation pack in LVGL's lv_translation_add_static layout: languages and tags are NULL
   terminated, translations is tags x languages, row-major. Storage must outlive LVGL. */
typedef struct
{
    const char *const *languages;
    const char *const *tags;
    const char *const *translations;
} meter_i18n_pack_t;
/* Reset the registry for a freshly initialised LVGL and select English. Safe to call again within
   one LVGL lifetime. */
bool meter_i18n_init(void);
/* Install the Product's language table. Entry i must carry id i: the id is the persisted language
   byte, so the numbering (and the default language) belongs to the Product, and the table may hold as
   many languages as the Product ships. It replaces the built-in default (0 = en, 1 = zh-CN), must
   outlive LVGL, and installing it again is a no-op. Codes must be unique and non-empty. */
bool meter_i18n_register_languages(const meter_i18n_language_t *table, size_t count);
size_t meter_i18n_language_count(void);
/* Add a translation pack (idempotent per pack). */
bool meter_i18n_register_pack(const meter_i18n_pack_t *pack);
/* The one place the language changes: LVGL sends LV_EVENT_TRANSLATION_LANGUAGE_CHANGED so every
   bound label re-translates and re-resolves its face. No-op if the language is already selected;
   false if the id was never registered. The persisted snapshot language is the source of truth. */
bool meter_i18n_set_language(meter_language_t language);
const char *meter_i18n_language_code(meter_language_t language);
meter_language_t meter_i18n_selected_language(void);
/* Selected language first, then the English column of the registered packs, then the tag itself. */
const char *meter_i18n_tr(const char *tag);
/* Same lookup for an explicit language (registered packs only). */
const char *meter_i18n_tr_in(meter_language_t language, const char *tag);
/* The selected language's text for tag, or NULL when no registered pack translates it: no fallback
   and no LVGL warning, for optional overrides such as a protocol-specific wording of a shared key. */
const char *meter_i18n_find(const char *tag);
/* Plural lookup: the text for the form the selected language uses for count, taken from the tags
   "<tag>.one" / "<tag>.other". Never returns a pointer into caller storage. */
const char *meter_i18n_tr_plural(const char *tag, unsigned count);

/* Typed template arguments. Templates use {0}..{9}; "{{" and "}}" are literal braces. Unlike printf
   the template is data, so a translator can reorder arguments and a bad template cannot read the
   stack. */
typedef struct
{
    enum
    {
        METER_I18N_ARG_U32,
        METER_I18N_ARG_S32,
        METER_I18N_ARG_STR
    } kind;
    union
    {
        uint32_t u32;
        int32_t s32;
        const char *str;
    } value;
} meter_i18n_arg_t;
static inline meter_i18n_arg_t meter_i18n_arg_u(uint32_t value)
{
    meter_i18n_arg_t arg = {METER_I18N_ARG_U32, {0}};
    arg.value.u32 = value;
    return arg;
}
static inline meter_i18n_arg_t meter_i18n_arg_s(int32_t value)
{
    meter_i18n_arg_t arg = {METER_I18N_ARG_S32, {0}};
    arg.value.s32 = value;
    return arg;
}
static inline meter_i18n_arg_t meter_i18n_arg_str(const char *value)
{
    meter_i18n_arg_t arg = {METER_I18N_ARG_STR, {0}};
    arg.value.str = value;
    return arg;
}
/* Expand a template into out (always NUL terminated when size > 0). A placeholder without an argument
   is copied verbatim. Returns the number of characters written. */
size_t meter_i18n_format(char *out, size_t size, const char *templ, const meter_i18n_arg_t *args,
                         size_t count);
/* Bind the translation tag and the language-dependent face of a label in one call. */
void meter_i18n_bind_text(lv_obj_t *label, const char *tag, meter_font_role_t role);
/* Bind only the face: the object re-resolves it on every language change and restores the face it was
   designed with when the language has none for the role. */
void meter_i18n_bind_font(lv_obj_t *obj, meter_font_role_t role);
/* LVGL's Arabic shaping (LV_USE_ARABIC_PERSIAN_CHARS) draws Unicode presentation forms, and many faces
   ship only some of them (MiSansArabic has no isolated forms). Inserting storage right after face in
   its fallback chain answers a missing isolated form with the base letter's glyph, which is the
   isolated shape. storage is caller-owned and must live as long as face. */
void meter_i18n_font_isolated_forms(lv_font_t *face, lv_font_t *storage);
/* The face the language registered for the role, or LVGL's Montserrat 14/24 when it has none. */
const lv_font_t *meter_font_get(meter_language_t language, meter_font_role_t role);
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language, meter_font_role_t role);
#endif
