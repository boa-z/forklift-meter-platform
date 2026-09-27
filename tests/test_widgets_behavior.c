#include "ui/common/i18n/meter_i18n.h"
#include "ui/common/widgets/meter_widgets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; } } while (0)
#define COLOR_EQ(a, b) (lv_color_to_u32(a) == lv_color_to_u32(b))

/* The hub is the only opaque disc in the gauge, so tests can locate it without child indices. */
static lv_obj_t *find_hub(lv_obj_t *root)
{
    lv_obj_t *match = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_get_style_bg_opa(child, 0) == LV_OPA_COVER &&
            lv_obj_get_style_radius(child, 0) == LV_RADIUS_CIRCLE) {
            if (match)
                return NULL;
            match = child;
        }
    }
    return match;
}

int main(void)
{
    lv_init();
    /* Without a display lv_obj_create(NULL) yields no screen and every object assertion is vacuous. */
    CHECK(lv_display_create(800, 480));
    CHECK(meter_i18n_init());
    meter_gauge_config_t config = {0, 50, 135, 405, 26, 5, 100, 35, "km/h"};
    lv_obj_t *root = lv_obj_create(NULL);
    meter_ring_t *ring = meter_ring_create(root, 0, 0, 120, "%");
    CHECK(ring);
    lv_obj_t *arc = lv_obj_get_child(root, 0);
    CHECK(lv_arc_get_bg_angle_start(arc) == 0);
    /* LVGL may normalize the full-circle endpoint to zero internally. */
    CHECK(lv_arc_get_bg_angle_end(arc) == 0 || lv_arc_get_bg_angle_end(arc) == 360);
    meter_ring_set_unit(ring, "kg");
    meter_ring_set_value(ring, 85);
    CHECK(meter_gauge_create(root, 0, 0, &config));
    config.min = 0.5f;
    size_t populated = meter_ui_object_count(root);
    CHECK(!meter_gauge_create(root, 0, 0, &config));
    CHECK(meter_ui_object_count(root) == populated);

    /* A rejected arc must be detected before it creates any LVGL object. */
    CHECK(!meter_arc_bar_create(root, 0, 0, 60, 200, 100, 5, "kg"));
    CHECK(!meter_arc_bar_create(root, 0, 0, 60, 0, 361, 5, "kg"));
    CHECK(!meter_arc_bar_create(root, 0, 0, 60, NAN, 360, 5, "kg"));
    CHECK(!meter_arc_bar_create(root, 0, 0, 60, 0, 360, 0, "kg"));
    CHECK(!meter_arc_bar_create(root, 0, 0, 8, 0, 360, 10, "kg"));
    CHECK(!meter_arc_bar_create(root, 0, 0, 0, 0, 360, 5, "kg"));
    CHECK(meter_ui_object_count(root) == populated);

    /* The Demo imperial speed scale stays integral, so the gauge contract accepts it. */
    lv_obj_t *panel = lv_obj_create(NULL);
    config.min = 0;
    meter_gauge_t *speed = meter_gauge_create(panel, 0, 0, &config);
    CHECK(speed);
    CHECK(meter_gauge_set_range(speed, 0, 32));
    meter_gauge_set_value(speed, 31.06855f);
    CHECK(!meter_gauge_set_range(speed, 0, 31.06855f));
    CHECK(!meter_gauge_set_range(speed, 0, 50.5f));

    /* Styling follows the stored hub pointer, not the position of a child. */
    lv_obj_t *gauge_root = lv_obj_get_child(panel, 0);
    lv_obj_t *hub = find_hub(gauge_root);
    CHECK(hub);
    meter_widget_style_t style = *meter_widget_style_default();
    style.text = lv_color_hex(0x112233);
    meter_gauge_set_style(speed, &style);
    CHECK(COLOR_EQ(lv_obj_get_style_bg_color(hub, 0), style.text));
    lv_obj_move_to_index(lv_obj_get_child(gauge_root, 2), 0);
    style.text = lv_color_hex(0x445566);
    meter_gauge_set_style(speed, &style);
    CHECK(find_hub(gauge_root) == hub);
    CHECK(COLOR_EQ(lv_obj_get_style_bg_color(hub, 0), style.text));
    lv_obj_delete(panel);

    /* Widgets have to be usable before a product supplies a theme. */
    const meter_widget_style_t *neutral = meter_widget_style_default();
    CHECK(neutral->value_font && neutral->label_font);
    CHECK(lv_color_to_u32(neutral->text) != 0 && lv_color_to_u32(neutral->primary) != 0);
    CHECK(!COLOR_EQ(neutral->primary, lv_color_hex(0x5de5ca)));
    lv_obj_t *bare = lv_obj_create(NULL);
    CHECK(meter_gauge_create(bare, 0, 0, &config));
    CHECK(meter_ring_create(bare, 0, 0, 120, "%"));
    CHECK(meter_arc_bar_create(bare, 0, 0, 60, 0, 360, 5, "kg"));
    CHECK(meter_linear_meter_create(bare, 0, 0, 120, 40, 0, 6, "m"));
    meter_value_label_t *load = meter_value_label_create(bare, 0, 0, "kg");
    CHECK(load);
    CHECK(meter_status_create(bare, 0, 0, "Seat", NULL));
    lv_obj_t *value = NULL, *arc_bar = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(bare); ++i) {
        lv_obj_t *child = lv_obj_get_child(bare, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class))
            value = child;
        if (lv_obj_check_type(child, &lv_arc_class))
            arc_bar = child;
    }
    CHECK(value && arc_bar);
    CHECK(lv_obj_get_style_text_font(value, 0) == neutral->value_font);
    CHECK(COLOR_EQ(lv_obj_get_style_text_color(value, 0), neutral->text));
    CHECK(COLOR_EQ(lv_obj_get_style_arc_color(arc_bar, LV_PART_MAIN), neutral->track));
    CHECK(COLOR_EQ(lv_obj_get_style_bg_color(find_hub(lv_obj_get_child(bare, 0)), 0), neutral->text));
    lv_obj_delete(bare);

    lv_obj_delete(root);
    lv_deinit();
    puts("Full-circle ring, setter redraw, integer gauge range, unstyled defaults and cleanup PASS");
    return 0;
}
