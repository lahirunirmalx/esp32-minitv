/**
 * @file app_main.cpp
 * @brief Dual-core supervisor for the Freenove ESP32 Mini TV.
 *
 *   - UI task  -> APP_CPU (core 1): LVGL rendering, Mooncake app lifecycle,
 *                 touch gestures, backlight idle management, portal overlay.
 *   - Net task -> PRO_CPU (core 0): WiFi connect/reconnect, BLE service,
 *                 captive portal.
 *
 * Any std::thread an app spawns also lands on core 0 (sdkconfig:
 * CONFIG_PTHREAD_TASK_CORE_DEFAULT=0), keeping all slow work off the UI core.
 *
 * Top-pad gestures (the device's only input):
 *   tap            -> next app (or wake the display if it was off)
 *   hold >= 3 s    -> open the captive portal (WiFi + shell color setup);
 *                     another 3 s hold inside the portal reboots/cancels
 */
#include <atomic>
#include <lvgl.h>
#include <mooncake.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_netif_sntp.h>
#include <esp_sleep.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "hal/hal_config.h"
#include "hal/backlight/backlight.h"
#include "hal/display/st7789.h"
#include "hal/display/lvgl_port.h"
#include "hal/touch/touch.h"
#include "net/wifi_manager.h"
#include "net/captive_portal.h"
#include "net/ble_service.h"
#include "ui/theme.h"
#include "ui/screen_app.h"
#include "apps/apps.h"
#include "apps/app_registry.h"

static const char* TAG = "main";

static constexpr BaseType_t kCoreUi = 1;   // APP_CPU
static constexpr BaseType_t kCoreNet = 0;  // PRO_CPU

// Cross-core signals.
static std::atomic<bool> s_portal_request{false};
static std::atomic<bool> s_portal_active{false};

// Full-screen overlay shown while the captive portal is up.
static void show_portal_overlay()
{
    const auto& pal = theme::palette();
    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_obj_set_pos(box, 0, 0);
    lv_obj_set_style_bg_color(box, pal.bg, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto line = [&](const char* txt, lv_color_t color, const lv_font_t* font) {
        lv_obj_t* l = lv_label_create(box);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, color, 0);
        if (font) lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_pad_ver(l, 4, 0);
    };
    line("WiFi Setup", pal.accent, &lv_font_montserrat_24);
    line("Join WiFi:", pal.text_muted, nullptr);
    line(captive_portal::AP_SSID, pal.text, &lv_font_montserrat_24);
    line("pass: 12345678", pal.text_muted, nullptr);
    line("then open", pal.text_muted, nullptr);
    line(captive_portal::PORTAL_URL, pal.accent, nullptr);
    line("hold 3s to cancel", pal.text_muted, nullptr);
}

static void ui_task(void*)
{
    apps::install_all();

    std::vector<int> cycle;
    for (const auto& e : app_registry::entries()) cycle.push_back(e.id);
    int cur = 0;
    if (!cycle.empty()) mooncake::GetMooncake().openApp(cycle[cur]);

    backlight::on();

    // Touch feedback: an accent-colored frame around the screen while the
    // pad is being touched, so a tap always has an immediate visible effect
    // (without it, cycling between similar apps can look like nothing
    // happened and beginners think the touch is broken).
    lv_obj_t* touch_frame = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(touch_frame);
    lv_obj_set_size(touch_frame, HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_obj_set_pos(touch_frame, 0, 0);
    lv_obj_set_style_bg_opa(touch_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(touch_frame, theme::palette().accent, 0);
    lv_obj_set_style_border_width(touch_frame, 4, 0);
    lv_obj_clear_flag(touch_frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(touch_frame, LV_OBJ_FLAG_HIDDEN);

    // First-boot hint: until WiFi has been set up, tell the user how to open
    // the portal (the gesture is otherwise undiscoverable). Removed once they
    // request the portal or credentials appear.
    lv_obj_t* hint = nullptr;
    if (!wifi_manager::peek_credentials()) {
        hint = lv_label_create(lv_layer_top());
        lv_label_set_text(hint, "hold top pad 3s to set up WiFi");
        lv_obj_set_style_text_color(hint, theme::palette().accent, 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_set_style_bg_color(hint, theme::palette().surface, 0);
        lv_obj_set_style_bg_opa(hint, LV_OPA_80, 0);
        lv_obj_set_style_pad_all(hint, 6, 0);
        lv_obj_set_style_radius(hint, 6, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    bool portal_shown = false;
    bool display_on = true;
    bool press_active = false;
    uint32_t press_start = 0;
    uint32_t last_interaction = ui::millis();

    while (true) {
        const uint32_t now = ui::millis();
        touch::tick(now);

        // Captive portal up -> setup overlay, stop the normal UI flow.
        if (s_portal_active.load()) {
            if (!portal_shown) {
                show_portal_overlay();
                backlight::on();
                portal_shown = true;
            }
            if (touch::take_long_press()) {
                ESP_LOGI(TAG, "long-press: cancelling portal, rebooting");
                esp_restart();
            }
            lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // >=3 s hold -> request the portal (net task picks it up).
        if (touch::take_long_press()) {
            ESP_LOGI(TAG, "long-press: requesting captive portal");
            if (hint) {
                lv_obj_delete(hint); // they found the gesture
                hint = nullptr;
            }
            s_portal_request.store(true);
            s_portal_active.store(true);
        }

        // Drop the hint once WiFi connects (set up from a previous boot).
        if (hint && wifi_manager::is_connected()) {
            lv_obj_delete(hint);
            hint = nullptr;
        }

        // Show the feedback frame while the pad is touched.
        const bool down = touch::pressed();
        if (down) lv_obj_clear_flag(touch_frame, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(touch_frame, LV_OBJ_FLAG_HIDDEN);

        // Tap, decided on release by hold duration.
        if (down && !press_active) {
            press_active = true;
            press_start = now;
        } else if (!down && press_active) {
            press_active = false;
            const uint32_t held = now - press_start;
            last_interaction = now;
            if (!display_on) {
                display_on = true;
                backlight::on();
            } else if (held < HAL_TOUCH_TAP_MAX_MS && !cycle.empty()) {
                // Tap: cycle to the next app.
                mooncake::GetMooncake().closeApp(cycle[cur]);
                cur = (cur + 1) % (int)cycle.size();
                mooncake::GetMooncake().openApp(cycle[cur]);
            }
        }

        // Idle: backlight off after the sleep window.
        if (display_on && (now - last_interaction > HAL_DISPLAY_SLEEP_MS)) {
            display_on = false;
            backlight::off();
        }

        // Extended idle: deep sleep (~10 uA-class instead of ~80 mA with the
        // radios up). Waking is a reboot — boot takes ~2 s, acceptable for a
        // desk gadget that's been ignored for 5 minutes.
        if (!display_on && (now - last_interaction > HAL_DEEP_SLEEP_MS)) {
            ESP_LOGI(TAG, "idle -> deep sleep (touch the pad to wake)");
            backlight::prepare_sleep();
            touch::prepare_wakeup();
            esp_sleep_enable_touchpad_wakeup();
            esp_deep_sleep_start(); // never returns
        }

        mooncake::GetMooncake().update();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void net_task(void*)
{
    wifi_manager::init();
    if (wifi_manager::has_credentials()) {
        wifi_manager::connect();
    }

    ble_service::start("MiniTV");

    bool sntp_started = false;
    uint32_t last_try = 0;
    while (true) {
        if (s_portal_request.load()) {
            captive_portal::run(); // never returns (reboots on save/cancel)
        }

        // SNTP once we're online: HTTPS certificate validation needs a real
        // clock (without it the device thinks it's 1970 and rejects every
        // cert), and time/clock apps get correct time for free.
        if (!sntp_started && wifi_manager::is_connected()) {
            esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_cfg));
            sntp_started = true;
            ESP_LOGI(TAG, "sntp started");
        }

        // Best-effort reconnect, ~10 s cadence.
        const uint32_t now = ui::millis();
        if (!wifi_manager::is_connected() && wifi_manager::has_credentials() &&
            (now - last_try > 10000)) {
            last_try = now;
            wifi_manager::connect();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    // NVS underpins WiFi calibration data, theme, and credentials.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Hardware bring-up, in dependency order: panel power first, then the
    // panel itself, then LVGL, touch, and the theme (needs NVS).
    backlight::init();
    st7789::chip_init();
    lvgl_port::init();
    touch::init();
    theme::init();

    xTaskCreatePinnedToCore(ui_task, "ui", 16384, nullptr, 5, nullptr, kCoreUi);
    xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 4, nullptr, kCoreNet);
}
