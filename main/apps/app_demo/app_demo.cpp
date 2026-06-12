/**
 * @file app_demo.cpp
 * @brief See app_demo.h.
 *
 * PATTERNS DEMONSTRATED (the minitv-create-app skill points here):
 *   1. Theme: all colors come from theme::palette() — nothing hardcoded.
 *   2. Lifecycle: build()/tick() via ui::ScreenApp; the base class owns the
 *      root container and deletes it (with all anims) on close.
 *   3. Animation: lv_anim with easing + infinite playback.
 *   4. Core split: tick() runs on the UI task (core 1) and only does cheap
 *      reads — wifi state is a flag the net task (core 0) maintains. Slow
 *      work (HTTP, JSON) never runs here; see web_client.h for the worker
 *      pattern.
 */
#include "app_demo.h"
#include "../../net/wifi_manager.h"

static void anim_y_cb(void* var, int32_t v) { lv_obj_set_y((lv_obj_t*)var, v); }
static void anim_arc_cb(void* var, int32_t v) { lv_arc_set_value((lv_obj_t*)var, v); }

void AppDemo::build(lv_obj_t* root)
{
    const auto& pal = theme::palette();
    setRefreshMs(500);

    // Bouncing headline in the accent color.
    _hello = theme::make_label(root, "Hello, Mini TV", false, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(_hello, pal.accent, 0);
    lv_obj_align(_hello, LV_ALIGN_TOP_MID, 0, 40);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _hello);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_values(&a, 40, 90);
    lv_anim_set_duration(&a, 900);
    lv_anim_set_playback_duration(&a, 900);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    // Sweeping arc in the second accent color.
    _arc = lv_arc_create(root);
    lv_obj_set_size(_arc, 120, 120);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, 30);
    lv_arc_set_rotation(_arc, 270);
    lv_arc_set_bg_angles(_arc, 0, 360);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_remove_style(_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(_arc, pal.surface, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arc, pal.accent_alt, LV_PART_INDICATOR);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, _arc);
    lv_anim_set_exec_cb(&b, anim_arc_cb);
    lv_anim_set_values(&b, 0, 100);
    lv_anim_set_duration(&b, 1600);
    lv_anim_set_playback_duration(&b, 1600);
    lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&b, lv_anim_path_ease_in_out);
    lv_anim_start(&b);

    // Status line fed from net-task state via a cheap flag read.
    _status = theme::make_label(root, "", true, &lv_font_montserrat_14);
    lv_obj_align(_status, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void AppDemo::tick()
{
    if (_status) {
        lv_label_set_text(_status,
                          wifi_manager::is_connected() ? "wifi: connected"
                                                       : "wifi: offline");
    }
}
