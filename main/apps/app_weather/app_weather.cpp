/**
 * @file app_weather.cpp
 * @brief See app_weather.h.
 *
 * THE WORKER PATTERN (copy this for any app that fetches data):
 *   build()    -> FIRST open only: starts one std::thread (lands on core 0
 *                 via sdkconfig) that loops forever: if _active, fetch ->
 *                 parse -> store atomics -> sleep. Every open after that
 *                 just flips _active back on.
 *   tick()     -> reads the atomics, repaints; never blocks, never fetches
 *   teardown() -> _active = false; the worker idles (it never exits)
 *
 * NEVER spawn the thread per open: rapid app cycling re-opens before the
 * old worker exits, every lap leaks a 16 KB stack, and when pthread
 * creation finally fails std::thread aborts the whole firmware (exceptions
 * are disabled). One immortal worker per app has nothing that can fail.
 *
 * web_client::get() and cJSON parsing both happen ONLY on the worker.
 */
#include "app_weather.h"
#include "../../net/wifi_manager.h"
#include "../../net/web_client.h"
#include <cJSON.h>
#include <thread>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Change these for your location (open-meteo needs no API key).
static constexpr const char* kLatitude = "6.93";    // Colombo, Sri Lanka
static constexpr const char* kLongitude = "79.85";
static constexpr int kPollMinutes = 10;

// WMO weather code -> short text (the codes open-meteo returns).
static const char* code_text(int code)
{
    if (code == 0) return "clear sky";
    if (code <= 2) return "partly cloudy";
    if (code == 3) return "overcast";
    if (code <= 48) return "fog";
    if (code <= 57) return "drizzle";
    if (code <= 67) return "rain";
    if (code <= 77) return "snow";
    if (code <= 82) return "rain showers";
    if (code <= 86) return "snow showers";
    if (code <= 99) return "thunderstorm";
    return "...";
}

void AppWeather::build(lv_obj_t* root)
{
    const auto& pal = theme::palette();
    setRefreshMs(1000);

    lv_obj_t* title = theme::make_label(root, "Weather", false, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(title, pal.accent, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    _temp = theme::make_label(root, "--", false, &lv_font_montserrat_48);
    lv_obj_align(_temp, LV_ALIGN_CENTER, 0, -16);

    _desc = theme::make_label(root, "", false, &lv_font_montserrat_14);
    lv_obj_set_style_text_color(_desc, pal.accent_alt, 0);
    lv_obj_align(_desc, LV_ALIGN_CENTER, 0, 24);

    _humidity = theme::make_label(root, "", true, &lv_font_montserrat_14);
    lv_obj_align(_humidity, LV_ALIGN_CENTER, 0, 46);

    _status = theme::make_label(root, "fetching...", true, &lv_font_montserrat_14);
    lv_obj_align(_status, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Re-opening with cached data: repaint it on the first tick.
    if (_code.load() >= 0) _fresh.store(true);

    // ---- core-0 worker: spawned once, lives for the app's lifetime ---------
    _active.store(true);
    if (_worker_started) return;
    _worker_started = true;
    std::thread([this] {
        const std::string url =
            std::string("https://api.open-meteo.com/v1/forecast?latitude=") + kLatitude +
            "&longitude=" + kLongitude +
            "&current=temperature_2m,relative_humidity_2m,weather_code";
        int sleep_s = 0;
        while (true) {
            if (_active.load() && sleep_s <= 0 && wifi_manager::is_connected()) {
                auto r = web_client::get(url);
                if (r.ok()) {
                    cJSON* json = cJSON_Parse(r.body.c_str());
                    const cJSON* cur = cJSON_GetObjectItem(json, "current");
                    const cJSON* t = cJSON_GetObjectItem(cur, "temperature_2m");
                    const cJSON* h = cJSON_GetObjectItem(cur, "relative_humidity_2m");
                    const cJSON* c = cJSON_GetObjectItem(cur, "weather_code");
                    if (cJSON_IsNumber(t) && cJSON_IsNumber(h) && cJSON_IsNumber(c)) {
                        _temp_x10.store((int)(t->valuedouble * 10));
                        _hum.store(h->valueint);
                        _code.store(c->valueint);
                        _error.store(false);
                        _fresh.store(true);
                    } else {
                        _error.store(true);
                    }
                    cJSON_Delete(json);
                } else {
                    _error.store(true);
                }
                sleep_s = kPollMinutes * 60;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (sleep_s > 0) sleep_s--;
        }
    }).detach();
}

void AppWeather::tick()
{
    if (_fresh.exchange(false)) {
        const int t = _temp_x10.load();
        lv_label_set_text_fmt(_temp, "%d.%d°C", t / 10, abs(t % 10));
        lv_label_set_text(_desc, code_text(_code.load()));
        lv_label_set_text_fmt(_humidity, "humidity %d%%", _hum.load());
        lv_label_set_text(_status, "");
    } else if (_error.load() && _code.load() < 0) {
        lv_label_set_text(_status, wifi_manager::is_connected() ? "fetch failed"
                                                                : "no wifi");
    }
}

void AppWeather::teardown()
{
    _active.store(false); // worker idles until the next open; it never exits
}
