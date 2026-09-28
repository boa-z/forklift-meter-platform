#include "ui/demo_internal.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    lv_init();
    assert(lv_display_create(800, 480));
    assert(meter_i18n_init() && demo_i18n_init());
    meter_ui_actions_t actions = {0};
    demo_ui_t *ui = demo_ui_create(lv_screen_active(), &actions);
    assert(ui);
    meter_update_widget_t *w = &ui->update_widget;
    size_t count = meter_ui_object_count(ui->root);
    meter_update_view_t view = {.visible = true,
                                .state = METER_UPDATE_DOWNLOADING,
                                .received = 37,
                                .total = 100,
                                .current_version = "current-a",
                                .target_version = "candidate-b"};
    for (unsigned language = 0; language < 2; ++language)
    {
        for (unsigned state = METER_UPDATE_IDLE; state <= METER_UPDATE_FAILED; ++state)
        {
            view.state = (meter_update_state_t)state;
            view.error = state == METER_UPDATE_FAILED ? 7 : 0;
            demo_ui_update_preview(ui, &view, (meter_language_t)language);
            lv_obj_update_layout(ui->root);
            assert(!lv_obj_has_flag(w->root, LV_OBJ_FLAG_HIDDEN));
            assert(lv_obj_get_width(w->root) == 800 && lv_obj_get_height(w->root) == 480);
            assert(lv_obj_get_style_radius(w->root, 0) == 0);
            assert(lv_bar_get_value(w->bar) == 37);
            assert(strstr(lv_label_get_text(w->versions), "candidate-b"));
            assert(lv_obj_has_flag(w->error, LV_OBJ_FLAG_HIDDEN) == (view.error == 0));
            assert(*lv_label_get_text(w->phase));
            assert(meter_ui_object_count(ui->root) == count);
        }
    }
    view.received = view.total;
    view.state = METER_UPDATE_TRANSFERRED;
    demo_ui_update(ui, &view, METER_LANGUAGE_EN);
    assert(lv_bar_get_value(w->bar) == 100);
    assert(strstr(lv_label_get_text(w->phase), "not yet verified"));
    assert(lv_obj_has_flag(w->preview, LV_OBJ_FLAG_HIDDEN));
    view.total = 0;
    demo_ui_update(ui, &view, METER_LANGUAGE_EN);
    assert(lv_bar_get_value(w->bar) == 0);
    view.visible = false;
    demo_ui_update(ui, &view, METER_LANGUAGE_EN);
    assert(lv_obj_has_flag(w->root, LV_OBJ_FLAG_HIDDEN));
    demo_ui_destroy(ui);
    lv_deinit();
    return 0;
}
