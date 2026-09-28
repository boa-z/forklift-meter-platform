#include "ui/ui.h"
#include "catalog/catalog.h"
#include <lvgl.h>
LV_FONT_DECLARE(reference_b_cjk_20);
static const char *const languages[]={"en","zh-CN",NULL};
static const char *const tags[]={"ref.drive","ref.power","ref.velocity","ref.torque","ref.remaining","ref.ambient","ref.unknown","ref.valid","ref.stale","ref.error",NULL};
static const char *const translations[]={"Drive","驱动","Power","能源","Velocity","速度","Torque","扭矩","Remaining","剩余电量","Ambient","环境温度","Unknown","未知","Valid","有效","Stale","过期","Error","错误"};
typedef struct { lv_obj_t *tabs,*values[4],*bar; meter_language_t language; } view_t;
static view_t view;
static void *create(void *parent,const meter_ui_actions_t *actions)
{
    (void)actions;
    if(!lv_translation_add_static(languages,tags,translations)) return NULL;
    view.tabs=lv_tabview_create(parent);
    view.language=(meter_language_t)255;
    lv_obj_set_size(view.tabs,800,480);
    lv_obj_set_style_text_font(view.tabs,&reference_b_cjk_20,0);
    lv_obj_set_style_bg_color(view.tabs,lv_color_hex(0xf4f2ff),0);
    lv_obj_set_style_text_color(view.tabs,lv_color_hex(0x302450),0);
    lv_tabview_set_tab_bar_size(view.tabs,64);
    for(unsigned page=0;page<2;++page)
    {
        lv_obj_t *tab=lv_tabview_add_tab(view.tabs,page ? "Power":"Drive");
        lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(tab,32,0);
        for(unsigned row=0;row<2;++row)
        {
            view.values[page*2+row]=lv_label_create(tab);
            lv_obj_set_width(view.values[page*2+row],680);
            lv_label_set_long_mode(view.values[page*2+row],LV_LABEL_LONG_CLIP);
        }
        if(!page)
        {
            view.bar=lv_bar_create(tab);
            lv_obj_set_size(view.bar,660,42);
            lv_bar_set_range(view.bar,0,120);
            lv_obj_set_style_bg_color(view.bar,lv_color_hex(0x7651be),LV_PART_INDICATOR);
        }
    }
    return &view;
}
static void present(void *ui,const meter_snapshot_t *s,uint32_t elapsed)
{
    (void)elapsed;
    view_t *v=ui;
    if(v->language!=s->language)
    {
        v->language=s->language;
        lv_translation_set_language(s->language==METER_LANGUAGE_ZH ? "zh-CN":"en");
        lv_tabview_set_tab_text(v->tabs,0,lv_translation_get("ref.drive"));
        lv_tabview_set_tab_text(v->tabs,1,lv_translation_get("ref.power"));
    }
    const meter_signal_id_t ids[]={PRODUCT_SPEED,REF_TORQUE,REF_SOC,REF_AMBIENT};
    const char *labels[]={"ref.velocity","ref.torque","ref.remaining","ref.ambient"};
    const char *states[]={"ref.unknown","ref.valid","ref.stale","ref.error"};
    const char *units[]={"m/s","Nm","%","C"};
    for(unsigned i=0;i<4;++i)
    {
        meter_value_t value=meter_snapshot_read(s,ids[i]);
        lv_label_set_text_fmt(v->values[i],"%s: %.1f %s  [%s]",lv_translation_get(labels[i]),(double)value.value,
            units[i],lv_translation_get(states[value.state]));
        if(!i) lv_bar_set_value(v->bar,value.state==METER_VALUE_VALID ? (int)(value.value*10):0,LV_ANIM_OFF);
    }
}
static void destroy(void *ui) { lv_obj_delete(((view_t *)ui)->tabs); }
unsigned reference_b_page(void *ui) { return lv_tabview_get_tab_active(((view_t *)ui)->tabs); }
const meter_ui_factory_t product_ui = {.create = create, .present = present, .destroy = destroy};
