/**
 * @file app_info.cpp
 * @brief See app_info.h. Everything here is a cheap read (atomics, mutexed
 *        copies, heap stats) — safe in tick() on the UI task.
 */
#include "app_info.h"
#include "../../net/wifi_manager.h"
#include "../../net/ble_service.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_app_desc.h>

void AppInfo::build(lv_obj_t* root)
{
    const auto& pal = theme::palette();
    setRefreshMs(1000);

    lv_obj_t* title = theme::make_label(root, "Device Info", false, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(title, pal.accent, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* card = theme::make_card(root);
    lv_obj_set_size(card, 216, 158);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    _wifi  = theme::make_label(card, "wifi: ...", false, &lv_font_montserrat_14);
    _ble   = theme::make_label(card, "ble: ...", false, &lv_font_montserrat_14);
    _heap  = theme::make_label(card, "heap: ...", false, &lv_font_montserrat_14);
    _theme = theme::make_label(card, "theme: ...", false, &lv_font_montserrat_14);

    // Static facts: set once. Min-ever heap is the leak detector — if it
    // keeps sinking across hours, something is leaking.
    char ver[40];
    snprintf(ver, sizeof(ver), "fw: v%s", esp_app_get_description()->version);
    theme::make_label(card, ver, true, &lv_font_montserrat_14);
    _min_heap = theme::make_label(card, "min heap: ...", true, &lv_font_montserrat_14);

    lv_obj_t* help = theme::make_label(root, "tap: next app | hold 3s: setup", true,
                                       &lv_font_montserrat_14);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void AppInfo::tick()
{
    if (wifi_manager::is_connected()) {
        lv_label_set_text_fmt(_wifi, "wifi: %s", wifi_manager::ip().c_str());
    } else {
        lv_label_set_text(_wifi, wifi_manager::peek_credentials()
                                     ? "wifi: connecting..."
                                     : "wifi: not set up");
    }
    lv_label_set_text_fmt(_ble, "ble: %s",
                          ble_service::is_connected() ? "connected" : "advertising");
    lv_label_set_text_fmt(_heap, "heap: %u KB free",
                          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    lv_label_set_text_fmt(_min_heap, "min heap: %u KB",
                          (unsigned)(esp_get_minimum_free_heap_size() / 1024));
    lv_label_set_text_fmt(_theme, "theme: %s", theme::shell_name());
}
