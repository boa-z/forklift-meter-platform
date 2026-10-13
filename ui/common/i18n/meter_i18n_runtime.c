#include "ui/common/i18n/meter_i18n_runtime.h"
#include "ui/common/formatter/meter_format.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Registry sizes: a handful of languages and packs is the realistic ceiling for a meter. */
#define MAX_LANGUAGES 8u
#define MAX_PACKS 8u
/* The only wording the framework owns, and only in English. Tags live in the reserved meter.*
   namespace so a Product pack can override a word without a naming clash with its own keys. */
static const char *const vocabulary_languages[] = {METER_LANGUAGE_CODE_EN, NULL};
static const char *const vocabulary_tags[] = {
    "meter.validity.live",    "meter.validity.stale", "meter.validity.unknown", "meter.validity.error",
    "meter.state.on",         "meter.state.off",      "meter.value.error",      NULL};
static const char *const vocabulary_texts[] = {"LIVE", "STALE", "NO DATA", "SENSOR ERROR", "ON", "OFF", "ERR %s"};
_Static_assert(sizeof(vocabulary_tags) / sizeof(vocabulary_tags[0]) == METER_TXT_COUNT + 1, "vocabulary tag count");
_Static_assert(sizeof(vocabulary_texts) / sizeof(vocabulary_texts[0]) == METER_TXT_COUNT,
               "vocabulary text count");
static const meter_i18n_pack_t vocabulary = {vocabulary_languages, vocabulary_tags, vocabulary_texts};
static meter_i18n_language_t languages[MAX_LANGUAGES];
static size_t language_total;
static const meter_i18n_pack_t *packs[MAX_PACKS];
static size_t pack_total;
static meter_font_resolver_t font_resolver;
/* A face binding remembers the face the object was designed with, so a language without a face
   for the role gives it back instead of leaving the previous language's face behind. */
typedef struct
{
    const lv_font_t *designed;
    meter_font_role_t role;
} face_binding_t;

static void reset_registry(void)
{
    memset(languages, 0, sizeof(languages));
    memset(packs, 0, sizeof(packs));
    language_total = 0u;
    pack_total = 0u;
    /* The two ids the persisted language byte always understands. */
    languages[language_total++] = (meter_i18n_language_t){METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL};
    languages[language_total++] = (meter_i18n_language_t){METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, NULL, NULL};
}
/* The built-in languages exist from the first use, so a Product that registers before
   meter_i18n_init() (or a unit test that skips it) still finds English and Chinese. */
static void ensure_registry(void)
{
    if (language_total == 0u)
        reset_registry();
}
static const meter_i18n_language_t *find_language(meter_language_t id)
{
    ensure_registry();
    for (size_t i = 0u; i < language_total; ++i)
        if (languages[i].id == id)
            return &languages[i];
    return NULL;
}
bool meter_i18n_init(void)
{
    /* LVGL clears its translation state on lv_init(); a selected language means this lifetime was
       already initialised and the registry (and the Product's packs) must be kept. */
    if (lv_translation_get_language() != NULL && pack_total != 0u && packs[0] == &vocabulary)
        return true;
    reset_registry();
    if (!meter_i18n_register_pack(&vocabulary))
        return false;
    lv_translation_set_language(METER_LANGUAGE_CODE_EN);
    return true;
}
bool meter_i18n_register_language(const meter_i18n_language_t *language)
{
    if (!language || !language->code || !language->code[0])
        return false;
    ensure_registry();
    for (size_t i = 0u; i < language_total; ++i)
    {
        if (languages[i].id == language->id)
        {
            languages[i] = *language;
            return true;
        }
    }
    if (language_total == MAX_LANGUAGES)
        return false;
    languages[language_total++] = *language;
    return true;
}
bool meter_i18n_register_pack(const meter_i18n_pack_t *pack)
{
    if (!pack || !pack->languages || !pack->tags || !pack->translations || !pack->languages[0])
        return false;
    ensure_registry();
    for (size_t i = 0u; i < pack_total; ++i)
        if (packs[i]->tags == pack->tags)
            return true;
    if (pack_total == MAX_PACKS || !lv_translation_add_static(pack->languages, pack->tags, pack->translations))
        return false;
    packs[pack_total++] = pack;
    return true;
}
const char *meter_i18n_language_code(meter_language_t language)
{
    const meter_i18n_language_t *known = find_language(language);
    if (known)
        return known->code;
    return METER_LANGUAGE_CODE_EN;
}
meter_language_t meter_i18n_selected_language(void)
{
    const char *code = lv_translation_get_language();
    for (size_t i = 0u; code && i < language_total; ++i)
        if (lv_strcmp(languages[i].code, code) == 0)
            return languages[i].id;
    return METER_LANGUAGE_EN;
}
bool meter_i18n_set_language(meter_language_t language)
{
    const meter_i18n_language_t *known = find_language(language);
    if (!known)
        return false;
    const char *current = lv_translation_get_language();
    if (current && lv_strcmp(current, known->code) == 0)
        return true;
    lv_translation_set_language(known->code);
    return true;
}
const char *meter_i18n_tag(meter_text_id_t id)
{
    return (unsigned)id < METER_TXT_COUNT ? vocabulary_tags[id] : "";
}
/* Newest pack first, the order lv_translation_get() itself walks. */
static const char *lookup(const char *code, const char *tag)
{
    for (size_t p = pack_total; p-- > 0u;)
    {
        const meter_i18n_pack_t *pack = packs[p];
        size_t columns = 0u, column = 0u;
        bool found = false;
        while (pack->languages[columns])
        {
            if (lv_strcmp(pack->languages[columns], code) == 0)
            {
                column = columns;
                found = true;
            }
            ++columns;
        }
        if (!found)
            continue;
        for (size_t row = 0u; pack->tags[row]; ++row)
        {
            if (lv_strcmp(pack->tags[row], tag) != 0)
                continue;
            const char *text = pack->translations[row * columns + column];
            if (text)
                return text;
            break;
        }
    }
    return NULL;
}
const char *meter_i18n_tr_in(meter_language_t language, const char *tag)
{
    if (!tag || !tag[0])
        return "";
    const char *text = lookup(meter_i18n_language_code(language), tag);
    if (!text)
        text = lookup(METER_LANGUAGE_CODE_EN, tag);
    return text ? text : tag;
}
const char *meter_i18n_tr(const char *tag)
{
    if (!tag || !tag[0])
        return "";
    /* LVGL answers for every pack, including ones a Product added with lv_translation_add_static
       directly; it hands back the very pointer it was given when it has no translation. */
    const char *text = lv_tr(tag);
    if (text != tag)
        return text;
    return meter_i18n_tr_in(meter_i18n_selected_language(), tag);
}
const char *meter_i18n_text(meter_text_id_t id)
{
    return meter_i18n_tr(meter_i18n_tag(id));
}
const char *meter_i18n_state(meter_value_state_t state)
{
    return meter_i18n_text(state == METER_VALUE_UNKNOWN ? METER_TXT_VALIDITY_UNKNOWN
                           : state == METER_VALUE_STALE ? METER_TXT_VALIDITY_STALE
                           : state == METER_VALUE_ERROR ? METER_TXT_VALIDITY_ERROR
                                                        : METER_TXT_VALIDITY_LIVE);
}
void meter_font_provider_set(meter_font_resolver_t resolver)
{
    font_resolver = resolver;
}
meter_font_role_t meter_font_role_of(const lv_font_t *font)
{
    return font && font->line_height >= 24 ? METER_FONT_VALUE : METER_FONT_LABEL;
}
/* The face a language registered for the role, or NULL when it has none. */
static const lv_font_t *substitute_face(meter_language_t language, meter_font_role_t role)
{
    const meter_i18n_language_t *known = find_language(language);
    if (known)
    {
        const lv_font_t *face = role == METER_FONT_VALUE ? known->value_font : known->label_font;
        if (face)
            return face;
    }
    return font_resolver ? font_resolver(language, role) : NULL;
}
const lv_font_t *meter_font_get(meter_language_t language, meter_font_role_t role)
{
    const lv_font_t *font = substitute_face(language, role);
    if (font)
        return font;
    return role == METER_FONT_VALUE ? &lv_font_montserrat_24 : &lv_font_montserrat_14;
}
void meter_i18n_apply_font(lv_obj_t *label, meter_language_t language, meter_font_role_t role)
{
    if (label)
        lv_obj_set_style_text_font(label, meter_font_get(language, role), 0);
}
static void resolve_face(lv_obj_t *obj, const face_binding_t *binding)
{
    const lv_font_t *face = substitute_face(meter_i18n_selected_language(), binding->role);
    if (!face)
        face = binding->designed;
    /* LVGL 9 invalidates on every style write, so an unchanged face must not be rewritten. */
    if (face && lv_obj_get_style_text_font(obj, 0) != face)
        lv_obj_set_style_text_font(obj, face, 0);
}
static void face_event(lv_event_t *event)
{
    face_binding_t *binding = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_DELETE)
    {
        lv_free(binding);
        return;
    }
    resolve_face(lv_event_get_target_obj(event), binding);
}
void meter_i18n_bind_font(lv_obj_t *obj, meter_font_role_t role)
{
    if (!obj)
        return;
    face_binding_t *binding = lv_malloc(sizeof(*binding));
    if (!binding)
        return;
    binding->designed = lv_obj_get_style_text_font(obj, 0);
    binding->role = role;
    lv_obj_add_event_cb(obj, face_event, LV_EVENT_TRANSLATION_LANGUAGE_CHANGED, binding);
    lv_obj_add_event_cb(obj, face_event, LV_EVENT_DELETE, binding);
    resolve_face(obj, binding);
}
void meter_i18n_bind_text(lv_obj_t *label, const char *tag, meter_font_role_t role)
{
    if (!label)
        return;
    meter_i18n_bind_font(label, role);
    if (tag && tag[0])
        lv_label_set_translation_tag(label, tag);
}
void meter_i18n_bind_label(lv_obj_t *label, const char *tag)
{
    if (!label)
        return;
    meter_i18n_bind_text(label, tag, meter_font_role_of(lv_obj_get_style_text_font(label, 0)));
}
/* Replace the first %s of a template; the catalog is the only source of templates, so a plain
   substitution keeps untrusted format strings out of printf. */
static void expand_template(char *out, size_t size, const char *format, const char *argument)
{
    const char *marker = strstr(format, "%s");
    if (!marker)
    {
        snprintf(out, size, "%s", format);
        return;
    }
    snprintf(out, size, "%.*s%s%s", (int)(marker - format), format, argument ? argument : "", marker + 2);
}
void meter_i18n_format_value(char *out, size_t size, float value, meter_value_state_t state,
                             const char *unit, unsigned decimals, meter_language_t language)
{
    if (!out || !size)
        return;
    if (state == METER_VALUE_ERROR || (state != METER_VALUE_UNKNOWN && !isfinite(value)))
    {
        expand_template(out, size, meter_i18n_tr_in(language, vocabulary_tags[METER_TXT_VALUE_ERROR]), unit);
        return;
    }
    meter_format_value(out, size, value, state == METER_VALUE_STALE ? METER_VALUE_VALID : state, unit, decimals);
    if (state == METER_VALUE_STALE)
    {
        size_t used = strlen(out);
        if (used < size)
            snprintf(out + used, size - used, " / %s",
                     meter_i18n_tr_in(language, vocabulary_tags[METER_TXT_VALIDITY_STALE]));
    }
}
