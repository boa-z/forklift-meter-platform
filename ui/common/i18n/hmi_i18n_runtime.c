#include "ui/common/i18n/hmi_i18n_runtime.h"
#include <lvgl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
/* The numbering a Product gets when it installs no table of its own. */
static const hmi_i18n_language_t default_languages[] = {
    {HMI_LANGUAGE_EN, HMI_LANGUAGE_CODE_EN, NULL, NULL},
    {HMI_LANGUAGE_ZH, HMI_LANGUAGE_CODE_ZH, NULL, NULL}};
static const hmi_i18n_language_t *languages = default_languages;
static size_t language_total = sizeof(default_languages) / sizeof(default_languages[0]);
static hmi_language_t selected = HMI_LANGUAGE_EN;
static const lv_i18n_language_pack_t *pack;
static uint32_t language_event;

/* Everything the service keeps per object: the translation tag of a label, the face the object was
   designed with (so a language without a face for the role gives it back instead of leaving the
   previous language's face behind), and membership of the language-change broadcast. */
typedef struct binding
{
    struct binding *next;
    lv_obj_t *obj;
    char *tag;
    const char *shown; /* the text this service last wrote: a different one means a plain write took over */
    const lv_font_t *designed;
    hmi_font_role_t role;
    bool has_role;
} binding_t;
static binding_t *bindings;

static void reset_registry(void)
{
    /* Objects of a previous LVGL lifetime are gone and took their bindings with them. */
    bindings = NULL;
    pack = NULL;
    __lv_i18n_reset();
    languages = default_languages;
    language_total = sizeof(default_languages) / sizeof(default_languages[0]);
    selected = HMI_LANGUAGE_EN;
}
/* Ids equal table positions (checked at registration), so the persisted byte indexes the table. */
static const hmi_i18n_language_t *find_language(hmi_language_t id)
{
    return (size_t)id < language_total ? &languages[id] : NULL;
}
/* lv_i18n keeps the current locale itself; a language the pack lacks reads the fallback language. */
static void apply_locale(void)
{
    if (!pack)
        return;
    if (lv_i18n_set_locale(languages[selected].code) != 0)
        (void)lv_i18n_set_locale(pack[0]->locale_name);
}
bool hmi_i18n_init(void)
{
    reset_registry();
    return true;
}
bool hmi_i18n_register_languages(const hmi_i18n_language_t *table, size_t count)
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
    if ((size_t)selected >= language_total)
        selected = HMI_LANGUAGE_EN;
    apply_locale();
    return true;
}
size_t hmi_i18n_language_count(void)
{
    return language_total;
}
bool hmi_i18n_register_pack(const lv_i18n_language_pack_t *tables)
{
    if (!tables || !tables[0])
        return false;
    if (pack)
        return pack == tables;
    if (lv_i18n_init(tables) != 0)
        return false;
    pack = tables;
    apply_locale();
    return true;
}
const char *hmi_i18n_language_code(hmi_language_t language)
{
    const hmi_i18n_language_t *known = find_language(language);
    if (known)
        return known->code;
    return HMI_LANGUAGE_CODE_EN;
}
hmi_language_t hmi_i18n_selected_language(void)
{
    return selected;
}
uint32_t hmi_i18n_language_event(void)
{
    if (language_event == 0u)
        language_event = lv_event_register_id();
    return language_event;
}
/* The face a language registered for the role, or NULL when it has none. */
static const lv_font_t *substitute_face(hmi_language_t language, hmi_font_role_t role)
{
    const hmi_i18n_language_t *known = find_language(language);
    if (!known)
        return NULL;
    return role == HMI_FONT_VALUE ? known->value_font : known->label_font;
}
static void resolve_face(lv_obj_t *obj, const binding_t *binding)
{
    const lv_font_t *face = substitute_face(hmi_i18n_selected_language(), binding->role);
    if (!face)
        face = binding->designed;
    /* LVGL 9 invalidates on every style write, so an unchanged face must not be rewritten. */
    if (face && lv_obj_get_style_text_font(obj, 0) != face)
        lv_obj_set_style_text_font(obj, face, 0);
}
static void retranslate(binding_t *binding)
{
    if (binding->has_role)
        resolve_face(binding->obj, binding);
    if (!binding->tag)
        return;
    const char *current = lv_label_get_text(binding->obj);
    if (binding->shown && lv_strcmp(current, binding->shown) != 0)
    {
        /* A plain lv_label_set_text() after the tag ends the text binding; the face binding stays. */
        lv_free(binding->tag);
        binding->tag = NULL;
        binding->shown = NULL;
        return;
    }
    binding->shown = hmi_i18n_tr(binding->tag);
    /* Same text, same pixels: skip the write that would invalidate the label. */
    if (lv_strcmp(current, binding->shown) != 0)
        lv_label_set_text(binding->obj, binding->shown);
}
bool hmi_i18n_set_language(hmi_language_t language)
{
    if (!find_language(language))
        return false;
    if (language == selected)
        return true;
    selected = language;
    apply_locale();
    for (binding_t *binding = bindings, *next; binding; binding = next)
    {
        next = binding->next;
        retranslate(binding);
        (void)lv_obj_send_event(binding->obj, HMI_EVENT_LANGUAGE_CHANGED, NULL);
    }
    return true;
}
static const char *phrase_find(const lv_i18n_phrase_t *phrases, const char *tag)
{
    for (; phrases && phrases->msg_id; ++phrases)
        if (lv_strcmp(phrases->msg_id, tag) == 0 && phrases->translation)
            return phrases->translation;
    return NULL;
}
static const lv_i18n_lang_t *find_table(const char *code)
{
    for (size_t i = 0u; pack && pack[i] != NULL; ++i)
        if (lv_strcmp(pack[i]->locale_name, code) == 0)
            return pack[i];
    return NULL;
}
const char *hmi_i18n_tr_in(hmi_language_t language, const char *tag)
{
    if (!tag || !tag[0])
        return "";
    const lv_i18n_lang_t *table = pack ? find_table(hmi_i18n_language_code(language)) : NULL;
    const char *text = table ? phrase_find(table->singulars, tag) : NULL;
    if (!text && pack)
        text = phrase_find(pack[0]->singulars, tag);
    return text ? text : tag;
}
const char *hmi_i18n_find(const char *tag)
{
    const lv_i18n_lang_t *table = tag && tag[0] && pack ? find_table(languages[selected].code) : NULL;
    return table ? phrase_find(table->singulars, tag) : NULL;
}
const char *hmi_i18n_tr(const char *tag)
{
    if (!tag || !tag[0])
        return "";
    return lv_i18n_get_text(tag);
}
const char *hmi_i18n_tr_plural(const char *tag, unsigned count)
{
    if (!tag || !tag[0])
        return "";
    return lv_i18n_get_text_plural(tag, (int32_t)count);
}
const lv_font_t *hmi_font_get(hmi_language_t language, hmi_font_role_t role)
{
    const lv_font_t *font = substitute_face(language, role);
    if (font)
        return font;
    return role == HMI_FONT_VALUE ? &lv_font_montserrat_24 : &lv_font_montserrat_14;
}
void hmi_i18n_apply_font(lv_obj_t *label, hmi_language_t language, hmi_font_role_t role)
{
    if (label)
        lv_obj_set_style_text_font(label, hmi_font_get(language, role), 0);
}
static void binding_event(lv_event_t *event)
{
    binding_t *binding = lv_event_get_user_data(event);
    for (binding_t **link = &bindings; *link; link = &(*link)->next)
        if (*link == binding)
        {
            *link = binding->next;
            break;
        }
    lv_free(binding->tag);
    lv_free(binding);
}
/* One binding per object, created on first use and released with the object. */
static binding_t *binding_of(lv_obj_t *obj)
{
    for (binding_t *binding = bindings; binding; binding = binding->next)
        if (binding->obj == obj)
            return binding;
    binding_t *binding = lv_malloc_zeroed(sizeof(*binding));
    if (!binding)
        return NULL;
    binding->obj = obj;
    binding->next = bindings;
    bindings = binding;
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_DELETE, binding);
    return binding;
}
void hmi_i18n_on_language_change(lv_obj_t *obj, void (*cb)(lv_event_t *), void *user_data)
{
    if (!obj || !cb || !binding_of(obj))
        return;
    lv_obj_add_event_cb(obj, cb, HMI_EVENT_LANGUAGE_CHANGED, user_data);
}
void hmi_i18n_label_set_tag(lv_obj_t *label, const char *tag)
{
    binding_t *binding = label && tag && tag[0] ? binding_of(label) : NULL;
    if (!binding)
        return;
    char *copy = lv_strdup(tag);
    if (!copy)
        return;
    lv_free(binding->tag);
    binding->tag = copy;
    binding->shown = NULL;
    retranslate(binding);
}
void hmi_i18n_bind_font(lv_obj_t *obj, hmi_font_role_t role)
{
    binding_t *binding = obj ? binding_of(obj) : NULL;
    if (!binding)
        return;
    if (!binding->has_role)
        binding->designed = lv_obj_get_style_text_font(obj, 0);
    binding->role = role;
    binding->has_role = true;
    resolve_face(obj, binding);
}
void hmi_i18n_bind_text(lv_obj_t *label, const char *tag, hmi_font_role_t role)
{
    if (!label)
        return;
    hmi_i18n_bind_font(label, role);
    hmi_i18n_label_set_tag(label, tag);
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
static const void *isolated_glyph_bitmap(lv_font_glyph_dsc_t *dsc, uint32_t letter, lv_draw_buf_t *draw_buf)
{
    const lv_font_t *self = dsc->resolved_font;
    dsc->resolved_font = self->user_data;
    const void *bitmap = dsc->resolved_font->get_glyph_bitmap(dsc, isolated_base(letter), draw_buf);
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
void hmi_i18n_font_isolated_forms(lv_font_t *face, lv_font_t *storage)
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
size_t hmi_i18n_format(char *out, size_t size, const char *templ, const hmi_i18n_arg_t *args,
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
            const hmi_i18n_arg_t *arg = &args[c[1] - '0'];
            if (arg->kind == HMI_I18N_ARG_STR)
                used = put_text(out, size, used, arg->value.str);
            else
            {
                /* 只有固定格式串交给 libc；翻译模板仍按带类型的位置参数解析。 */
                char number[12];
                if (arg->kind == HMI_I18N_ARG_S32)
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
