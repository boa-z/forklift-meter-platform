#ifndef METER_I18N_RUNTIME_H
#define METER_I18N_RUNTIME_H
#include <stddef.h>
#include "contracts/meter_domain.h"
typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_font_t lv_font_t;
/* Language codes name the columns of a translation pack. The framework ships no wording for any
   language except English: a Product registers its own languages, packs and faces, so adding a
   language never edits the framework. */
#define METER_LANGUAGE_CODE_EN "en"
#define METER_LANGUAGE_CODE_ZH "zh-CN"
/* Vocabulary the framework widgets render themselves. Only the English text is built in; a Product
   overrides a word by registering a pack that contains the same tag (tags are meter.*). */
typedef enum
{
    METER_TXT_VALIDITY_LIVE,
    METER_TXT_VALIDITY_STALE,
    METER_TXT_VALIDITY_UNKNOWN,
    METER_TXT_VALIDITY_ERROR,
    METER_TXT_STATE_ON,
    METER_TXT_STATE_OFF,
    METER_TXT_VALUE_ERROR, /* template with one %s for the unit, e.g. "ERR %s" */
    METER_TXT_COUNT
} meter_text_id_t;
/* Size classes a widget asks for. The product decides which face answers them, so
   widgets never name a product font. */
typedef enum { METER_FONT_LABEL, METER_FONT_VALUE } meter_font_role_t;
typedef const lv_font_t *(*meter_font_resolver_t)(meter_language_t language, meter_font_role_t role);
/* One selectable language. id is the persisted language byte (meter_language_t, extended by the
   Product); code is the pack column. A NULL face means "keep the face the widget was designed with"
   for that role. */
typedef struct
{
    meter_language_t id;
    const char *code;
    const lv_font_t *label_font;
    const lv_font_t *value_font;
} meter_i18n_language_t;
/* A static translation pack in LVGL's lv_translation_add_static layout: languages and tags are NULL
   terminated, translations is tags x languages, row-major. Storage must outlive LVGL. */
typedef struct
{
    const char *const *languages;
    const char *const *tags;
    const char *const *translations;
} meter_i18n_pack_t;
/* Reset the registry for a freshly initialised LVGL and register the English vocabulary. Safe to
   call again within one LVGL lifetime. */
bool meter_i18n_init(void);
/* Register or replace a selectable language / add a translation pack (idempotent per pack). */
bool meter_i18n_register_language(const meter_i18n_language_t *language);
bool meter_i18n_register_pack(const meter_i18n_pack_t *pack);
/* The one place the language changes: LVGL sends LV_EVENT_TRANSLATION_LANGUAGE_CHANGED so every
   bound label re-translates and re-resolves its face. No-op if the language is already selected;
   false if the id was never registered. The persisted snapshot language is the source of truth. */
bool meter_i18n_set_language(meter_language_t language);
const char *meter_i18n_language_code(meter_language_t language);
meter_language_t meter_i18n_selected_language(void);
const char *meter_i18n_tag(meter_text_id_t id);
const char *meter_i18n_text(meter_text_id_t id);
/* Selected language first, then the English column of the registered packs, then the tag itself. */
const char *meter_i18n_tr(const char *tag);
/* Same lookup for an explicit language (registered packs only). */
const char *meter_i18n_tr_in(meter_language_t language, const char *tag);
const char *meter_i18n_state(meter_value_state_t state);
/* Bind the translation tag and the language-dependent face of a label in one call. */
void meter_i18n_bind_text(lv_obj_t *label, const char *tag, meter_font_role_t role);
/* Bind only the face: the object re-resolves it on every language change and restores the face it was
   designed with when the language has none for the role. */
void meter_i18n_bind_font(lv_obj_t *obj, meter_font_role_t role);
/* Legacy form of meter_i18n_bind_text: the role is guessed from the label's current font. */
void meter_i18n_bind_label(lv_obj_t *label, const char *tag);
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language);
/* Legacy resolver hook; a language registered with meter_i18n_register_language takes precedence. */
void meter_font_provider_set(meter_font_resolver_t resolver);
meter_font_role_t meter_font_role_of(const lv_font_t *font);
const lv_font_t *meter_font_get(meter_language_t language, meter_font_role_t role);
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language, meter_font_role_t role);
#endif
