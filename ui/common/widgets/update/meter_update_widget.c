#include "ui/common/widgets/update/meter_update_widget.h"
#include <string.h>
static void text_set(lv_obj_t *label, const char *text)
{
    const char *value = text ? text : "";
    if (strcmp(lv_label_get_text(label), value))
        lv_label_set_text(label, value);
}
static lv_obj_t *label_create(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(94));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "");
    return label;
}
bool meter_update_widget_create(meter_update_widget_t *w, lv_obj_t *parent)
{
    if (!w || !parent)
        return false;
    memset(w, 0, sizeof(*w));
    w->root = lv_obj_create(parent);
    lv_obj_remove_style_all(w->root);
    lv_obj_set_size(w->root, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(w->root, 0, 0);
    lv_obj_set_style_bg_color(w->root, lv_color_hex(0x091a25), 0);
    lv_obj_set_style_bg_opa(w->root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->root, 0, 0);
    lv_obj_set_style_text_color(w->root, lv_color_hex(0xe9f2f5), 0);
    lv_obj_set_style_pad_all(w->root, 28, 0);
    lv_obj_set_style_pad_row(w->root, 18, 0);
    lv_obj_set_scrollable(w->root, false);
    lv_obj_set_clickable(w->root, true);
    lv_obj_set_hidden(w->root, true);
    lv_obj_set_flex_flow(w->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(w->root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w->preview = label_create(w->root);
    lv_obj_set_style_text_color(w->preview, lv_color_hex(0xf3ba65), 0);
    w->title = label_create(w->root);
    w->phase = label_create(w->root);
    w->bar = lv_bar_create(w->root);
    lv_obj_set_size(w->bar, lv_pct(82), 18);
    lv_bar_set_range(w->bar, 0, 100);
    lv_obj_set_style_bg_color(w->bar, lv_color_hex(0x244050), LV_PART_MAIN);
    lv_obj_set_style_bg_color(w->bar, lv_color_hex(0x5de5ca), LV_PART_INDICATOR);
    w->percent = label_create(w->root);
    w->versions = label_create(w->root);
    w->error = label_create(w->root);
    lv_obj_set_style_text_color(w->error, lv_color_hex(0xff7979), 0);
    w->note = label_create(w->root);
    lv_obj_set_style_text_color(w->note, lv_color_hex(0x9cb5c4), 0);
    return true;
}
void meter_update_widget_present(meter_update_widget_t *w, const meter_update_view_t *v,
                                 const meter_update_widget_text_t *t, meter_language_t language)
{
    if (!w || !w->root || !v || !t)
        return;
    if (!v->visible)
    {
        lv_obj_set_hidden(w->root, true);
        return;
    }
    lv_obj_set_hidden(w->root, false);
    lv_obj_t *labels[] = {w->title, w->phase, w->percent, w->versions, w->error, w->note, w->preview};
    for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i)
    {
        const lv_font_t *font = meter_font_get(language, i < 3 ? METER_FONT_VALUE : METER_FONT_LABEL);
        if (font && lv_obj_get_style_text_font(labels[i], 0) != font)
            lv_obj_set_style_text_font(labels[i], font, 0);
    }
    text_set(w->title, t->title);
    text_set(w->phase, t->phase);
    text_set(w->note, t->note);
    text_set(w->preview, t->preview);
    if (t->preview && *t->preview)
        lv_obj_set_hidden(w->preview, false);
    else
        lv_obj_set_hidden(w->preview, true);
    uint32_t percent = v->total ? (uint32_t)((uint64_t)v->received * 100u / v->total) : 0u;
    if (percent > 100u)
        percent = 100u;
    if (lv_bar_get_value(w->bar) != (int32_t)percent)
        lv_bar_set_value(w->bar, (int32_t)percent, LV_ANIM_OFF);
    char text[256];
    lv_snprintf(text, sizeof(text), "%u%%   %u / %u B", (unsigned)percent, (unsigned)v->received,
                (unsigned)v->total);
    text_set(w->percent, text);
    lv_snprintf(text, sizeof(text), "%s: %.47s\n%s: %.47s", t->current ? t->current : "", v->current_version,
                t->target ? t->target : "", v->target_version);
    text_set(w->versions, text);
    if (v->error)
    {
        lv_snprintf(text, sizeof(text), "%s: %u", t->error ? t->error : "", (unsigned)v->error);
        text_set(w->error, text);
        lv_obj_set_hidden(w->error, false);
    }
    else
        lv_obj_set_hidden(w->error, true);
    lv_color_t color = lv_color_hex(v->state == METER_UPDATE_FAILED ? 0xff7979 : 0x5de5ca);
    if (lv_color_to_u32(lv_obj_get_style_text_color(w->phase, 0)) != lv_color_to_u32(color))
        lv_obj_set_style_text_color(w->phase, color, 0);
}
