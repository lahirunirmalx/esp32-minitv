/**
 * @file screen_app.h
 * @brief Base class for full-screen Mini TV apps.
 *
 * Each app is an independent mooncake::AppAbility. The navigator (app_main
 * UI task) opens one at a time: onOpen() builds a themed full-screen root
 * container on the active screen and the subclass widgets; onRunning()
 * repaints on a throttled cadence; onClose() tears everything down.
 *
 * Subclasses implement build()/tick()/teardown() and never touch lifecycle:
 *
 *   class AppFoo : public ui::ScreenApp {
 *   public:
 *       AppFoo() { setAppInfo().name = "foo"; }
 *   protected:
 *       void build(lv_obj_t* root) override;  // create widgets under root
 *       void tick() override;                 // repaint from latest data
 *       void teardown() override;             // stop timers etc. (optional)
 *   };
 *
 * THREADING: build()/tick()/teardown() run on the UI task (core 1) and must
 * never block. Slow work (HTTP, JSON, heavy math) belongs on core 0 — spawn
 * a std::thread (pinned to core 0 via sdkconfig) or a FreeRTOS task and hand
 * results back through a queue or std::atomic; read them in tick().
 */
#pragma once

#include "theme.h"
#include "../hal/hal_config.h"
#include <lvgl.h>
#include <mooncake.h>
#include <esp_timer.h>
#include <cstdint>

namespace ui {

inline uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

class ScreenApp : public mooncake::AppAbility {
public:
    void onOpen() override
    {
        _root = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(_root);
        lv_obj_set_size(_root, HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
        lv_obj_set_pos(_root, 0, 0);
        // Theme is re-applied on every open, so a shell/palette change shows
        // after an app cycle without rebooting.
        theme::apply_screen(_root);
        lv_obj_clear_flag(_root, LV_OBJ_FLAG_CLICKABLE);
        build(_root);
        _last_ms = 0;
        onRunning(); // first paint
    }

    void onRunning() override
    {
        const uint32_t now = millis();
        if (_last_ms != 0 && now - _last_ms < _refresh_ms) return;
        _last_ms = now;
        tick();
    }

    void onClose() override
    {
        teardown();
        if (_root) {
            lv_obj_delete(_root); // children + their running anims go too
            _root = nullptr;
        }
    }

protected:
    virtual void build(lv_obj_t* root) = 0; // create widgets under root
    virtual void tick() {}                  // repaint from latest data
    virtual void teardown() {}              // stop anims/timers before root dies

    void setRefreshMs(uint32_t ms) { _refresh_ms = ms; }

    lv_obj_t* _root = nullptr;

private:
    uint32_t _last_ms = 0;
    uint32_t _refresh_ms = 500;
};

} // namespace ui
