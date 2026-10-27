#include "ui/common/i18n/meter_i18n_runtime.h"
#include <lvgl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
/* A handful of packs is the realistic ceiling for a meter; languages live in the Product's table. */
#define MAX_PACKS 8u
/* The numbering a Product gets when it installs no table of its own. */
static const meter_i18n_language_t default_languages[] = {
    {METER_LANGUAGE_EN, METER_LANGUAGE_CODE_EN, NULL, NULL, METER_PLURAL_ONE_OTHER},
    {METER_LANGUAGE_ZH, METER_LANGUAGE_CODE_ZH, NULL, NULL, METER_PLURAL_OTHER_ONLY}};
static const meter_i18n_language_t *languages = default_languages;
static size_t language_total = sizeof(default_languages) / sizeof(default_languages[0]);
static const meter_i18n_pack_t *packs[MAX_PACKS];
static size_t pack_total;
/* A face binding remembers the face the object was designed with, so a language without a face
   for the role gives it back instead of leaving the previous language's face behind. */
typedef struct
{
    const lv_font_t *designed;
    meter_font_role_t role;
} face_binding_t;

static void reset_registry(void)
{
    memset(packs, 0, sizeof(packs));
    pack_total = 0u;
    languages = default_languages;
    language_total = sizeof(default_languages) / sizeof(default_languages[0]);
}
/* Ids equal table positions (checked at registration), so the persisted byte indexes the table. */
static const meter_i18n_language_t *find_language(meter_language_t id)
{
    return (size_t)id < language_total ? &languages[id] : NULL;
}
bool meter_i18n_init(void)
{
    /* LVGL clears its translation state on lv_init(); a selected language means this lifetime was
       already initialised and the registry (and the Product's packs) must be kept. */
    if (lv_translation_get_language() != NULL)
        return true;
    reset_registry();
    lv_translation_set_language(METER_LANGUAGE_CODE_EN);
    return true;
}
bool meter_i18n_register_languages(const meter_i18n_language_t *table, size_t count)
{
    if (!table || count == 0u)
        return false;
    for (size_t i = 0u; i < count; ++i)
    {
        if ((size_t)table[i].id != i || !table[i].code || !table[i].code[0])
            return false;
        for (size_t j = 0u; j < i; ++j)
            if (lv_strcmp(table[j].code, table[i].code) == 0)
                return false;
    }
    languages = table;
    language_total = count;
    return true;
}
size_t meter_i18n_language_count(void)
{
    return language_total;
}
bool meter_i18n_register_pack(const meter_i18n_pack_t *pack)
{
    if (!pack || !pack->languages || !pack->tags || !pack->translations || !pack->languages[0])
        return false;
    for (size_t i = 0u; i < pack_total; ++i)
        if (packs[i]->tags == pack->tags)
            return true;
    if (pack_total == MAX_PACKS ||
        !lv_translation_add_static(pack->languages, pack->tags, pack->translations))
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
const char *meter_i18n_find(const char *tag)
{
    const char *code = lv_translation_get_language();
    return tag && tag[0] && code ? lookup(code, tag) : NULL;
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
const char *meter_i18n_tr_plural(const char *tag, unsigned count)
{
    if (!tag || !tag[0])
        return "";
    const meter_i18n_language_t *known = find_language(meter_i18n_selected_language());
    const bool one = known && known->plural == METER_PLURAL_ONE_OTHER && count == 1u;
    char variant[96];
    /* The variant buffer is only a lookup key: a miss makes lv_tr() hand the key back, and that
       pointer must never escape, so a miss returns the base tag instead. */
    if (one)
    {
        snprintf(variant, sizeof(variant), "%s.one", tag);
        const char *text = meter_i18n_tr(variant);
        if (text != variant)
            return text;
    }
    snprintf(variant, sizeof(variant), "%s.other", tag);
    const char *text = meter_i18n_tr(variant);
    return text != variant ? text : tag;
}
/* The face a language registered for the role, or NULL when it has none. */
static const lv_font_t *substitute_face(meter_language_t language, meter_font_role_t role)
{
    const meter_i18n_language_t *known = find_language(language);
    if (!known)
        return NULL;
    return role == METER_FONT_VALUE ? known->value_font : known->label_font;
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
/* Isolated presentation form -> base letter (Unicode decomposition <isolated>), sorted by form. */
static const uint16_t isolated_forms[][2] = {
    {0xFB50u, 0x0671u}, {0xFB52u, 0x067Bu}, {0xFB56u, 0x067Eu}, {0xFB5Au, 0x0680u}, {0xFB5Eu, 0x067Au},
    {0xFB62u, 0x067Fu}, {0xFB66u, 0x0679u}, {0xFB6Au, 0x06A4u}, {0xFB6Eu, 0x06A6u}, {0xFB72u, 0x0684u},
    {0xFB76u, 0x0683u}, {0xFB7Au, 0x0686u}, {0xFB7Eu, 0x0687u}, {0xFB82u, 0x068Du}, {0xFB84u, 0x068Cu},
    {0xFB86u, 0x068Eu}, {0xFB88u, 0x0688u}, {0xFB8Au, 0x0698u}, {0xFB8Cu, 0x0691u}, {0xFB8Eu, 0x06A9u},
    {0xFB92u, 0x06AFu}, {0xFB96u, 0x06B3u}, {0xFB9Au, 0x06B1u}, {0xFB9Eu, 0x06BAu}, {0xFBA0u, 0x06BBu},
    {0xFBA4u, 0x06C0u}, {0xFBA6u, 0x06C1u}, {0xFBAAu, 0x06BEu}, {0xFBAEu, 0x06D2u}, {0xFBB0u, 0x06D3u},
    {0xFBD3u, 0x06ADu}, {0xFBD7u, 0x06C7u}, {0xFBD9u, 0x06C6u}, {0xFBDBu, 0x06C8u}, {0xFBDDu, 0x0677u},
    {0xFBDEu, 0x06CBu}, {0xFBE0u, 0x06C5u}, {0xFBE2u, 0x06C9u}, {0xFBE4u, 0x06D0u}, {0xFBFCu, 0x06CCu},
    {0xFE80u, 0x0621u}, {0xFE81u, 0x0622u}, {0xFE83u, 0x0623u}, {0xFE85u, 0x0624u}, {0xFE87u, 0x0625u},
    {0xFE89u, 0x0626u}, {0xFE8Du, 0x0627u}, {0xFE8Fu, 0x0628u}, {0xFE93u, 0x0629u}, {0xFE95u, 0x062Au},
    {0xFE99u, 0x062Bu}, {0xFE9Du, 0x062Cu}, {0xFEA1u, 0x062Du}, {0xFEA5u, 0x062Eu}, {0xFEA9u, 0x062Fu},
    {0xFEABu, 0x0630u}, {0xFEADu, 0x0631u}, {0xFEAFu, 0x0632u}, {0xFEB1u, 0x0633u}, {0xFEB5u, 0x0634u},
    {0xFEB9u, 0x0635u}, {0xFEBDu, 0x0636u}, {0xFEC1u, 0x0637u}, {0xFEC5u, 0x0638u}, {0xFEC9u, 0x0639u},
    {0xFECDu, 0x063Au}, {0xFED1u, 0x0641u}, {0xFED5u, 0x0642u}, {0xFED9u, 0x0643u}, {0xFEDDu, 0x0644u},
    {0xFEE1u, 0x0645u}, {0xFEE5u, 0x0646u}, {0xFEE9u, 0x0647u}, {0xFEEDu, 0x0648u}, {0xFEEFu, 0x0649u},
    {0xFEF1u, 0x064Au}};
static uint32_t isolated_base(uint32_t letter)
{
    size_t low = 0u, high = sizeof(isolated_forms) / sizeof(isolated_forms[0]);
    while (low < high)
    {
        size_t middle = (low + high) / 2u;
        if (isolated_forms[middle][0] < letter)
            low = middle + 1u;
        else
            high = middle;
    }
    return low < sizeof(isolated_forms) / sizeof(isolated_forms[0]) && isolated_forms[low][0] == letter
               ? isolated_forms[low][1]
               : 0u;
}
static bool isolated_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t letter,
                               uint32_t next)
{
    (void)next;
    const lv_font_t *face = font->user_data;
    const uint32_t base = isolated_base(letter);
    return base != 0u && face->get_glyph_dsc(face, dsc, base, 0u);
}
/* LVGL records this font as the glyph's owner; drawing and releasing the glyph belong to the face. */
static const void *isolated_glyph_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf)
{
    const lv_font_t *self = dsc->resolved_font;
    dsc->resolved_font = self->user_data;
    const void *bitmap = dsc->resolved_font->get_glyph_bitmap(dsc, draw_buf);
    dsc->resolved_font = self;
    return bitmap;
}
static void isolated_release(const lv_font_t *font, lv_font_glyph_dsc_t *dsc)
{
    const lv_font_t *face = font->user_data;
    if (!face->release_glyph)
        return;
    dsc->resolved_font = face;
    face->release_glyph(face, dsc);
    dsc->resolved_font = font;
}
void meter_i18n_font_isolated_forms(lv_font_t *face, lv_font_t *storage)
{
    if (!face || !storage || face == storage || !face->get_glyph_dsc || !face->get_glyph_bitmap)
        return;
    *storage = *face; /* the face's metrics, so a substituted glyph sits on the same baseline */
    storage->get_glyph_dsc = isolated_glyph_dsc;
    storage->get_glyph_bitmap = isolated_glyph_bitmap;
    storage->release_glyph = isolated_release;
    storage->user_data = face;
    storage->fallback = face->fallback;
    face->fallback = storage;
}
static size_t put_char(char *out, size_t size, size_t used, char c)
{
    if (used + 1u < size)
        out[used] = c;
    return used + 1u;
}
static size_t put_text(char *out, size_t size, size_t used, const char *text)
{
    for (; text && *text; ++text)
        used = put_char(out, size, used, *text);
    return used;
}
size_t meter_i18n_format(char *out, size_t size, const char *templ, const meter_i18n_arg_t *args,
                         size_t count)
{
    if (!out || size == 0u)
        return 0u;
    size_t used = 0u;
    for (const char *c = templ ? templ : ""; *c; ++c)
    {
        if ((c[0] == '{' && c[1] == '{') || (c[0] == '}' && c[1] == '}'))
        {
            used = put_char(out, size, used, c[0]);
            ++c;
        }
        else if (c[0] == '{' && c[1] >= '0' && c[1] <= '9' && c[2] == '}' && args &&
                 (size_t)(c[1] - '0') < count)
        {
            const meter_i18n_arg_t *arg = &args[c[1] - '0'];
            if (arg->kind == METER_I18N_ARG_STR)
                used = put_text(out, size, used, arg->value.str);
            else
            {
                /* 只有固定格式串交给 libc；翻译模板仍按带类型的位置参数解析。 */
                char number[12];
                if (arg->kind == METER_I18N_ARG_S32)
                    snprintf(number, sizeof(number), "%" PRId32, arg->value.s32);
                else
                    snprintf(number, sizeof(number), "%" PRIu32, arg->value.u32);
                used = put_text(out, size, used, number);
            }
            c += 2;
        }
        else
            used = put_char(out, size, used, *c);
    }
    out[used < size ? used : size - 1u] = '\0';
    return used < size ? used : size - 1u;
}
