#ifndef METER_I18N_RUNTIME_H
#define METER_I18N_RUNTIME_H
#include "contracts/meter_domain.h"
typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_font_t lv_font_t;
/* Every translation pack is keyed by these codes, so a product extends the
   language list instead of renaming what the runtime already understands. */
#define METER_LANGUAGE_CODE_EN "en"
#define METER_LANGUAGE_CODE_ZH "zh-CN"
/* Validity and boolean vocabulary every product renders; product wording stays in the product. */
typedef enum
{
    METER_TXT_VALIDITY_LIVE,
    METER_TXT_VALIDITY_STALE,
    METER_TXT_VALIDITY_UNKNOWN,
    METER_TXT_VALIDITY_ERROR,
    METER_TXT_STATE_ON,
    METER_TXT_STATE_OFF,
    METER_TXT_COUNT
} meter_text_id_t;
/* Size classes a widget asks for. The product decides which face answers them, so
   widgets never name a product font. */
typedef enum { METER_FONT_LABEL, METER_FONT_VALUE } meter_font_role_t;
typedef const lv_font_t *(*meter_font_resolver_t)(meter_language_t language, meter_font_role_t role);
bool meter_i18n_init(void);
const char *meter_i18n_language_code(meter_language_t language);
meter_language_t meter_i18n_selected_language(void);
const char *meter_i18n_tag(meter_text_id_t id);
const char *meter_i18n_text(meter_text_id_t id);
const char *meter_i18n_tr(const char *tag);
const char *meter_i18n_state(meter_value_state_t state);
void meter_i18n_bind_label(lv_obj_t *label, const char *tag);
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language);
void meter_font_provider_set(meter_font_resolver_t resolver);
meter_font_role_t meter_font_role_of(const lv_font_t *font);
const lv_font_t *meter_font_get(meter_language_t language, meter_font_role_t role);
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language, meter_font_role_t role);
#endif
