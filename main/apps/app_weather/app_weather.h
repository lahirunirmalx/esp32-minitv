/**
 * @file app_weather.h
 * @brief Weather app — THE reference implementation of the network-fetch
 *        pattern: a core-0 worker thread polls an HTTPS API and hands
 *        results to the UI through atomics; tick() (core 1) only reads.
 *
 * Data: open-meteo.com (free, no API key). Change the coordinates at the
 * top of app_weather.cpp for your location.
 */
#pragma once
#include "../../ui/screen_app.h"
#include <atomic>

class AppWeather : public ui::ScreenApp {
public:
    AppWeather() { setAppInfo().name = "weather"; }

protected:
    void build(lv_obj_t* root) override;
    void tick() override;
    void teardown() override;

private:
    lv_obj_t* _temp = nullptr;
    lv_obj_t* _desc = nullptr;
    lv_obj_t* _humidity = nullptr;
    lv_obj_t* _status = nullptr;

    // Worker -> UI handoff. Plain values only; the UI never touches the
    // HTTP response. fresh flips true when a new reading landed.
    //
    // THREAD-LIFETIME RULE (learned from a real crash): the worker is
    // spawned ONCE per app lifetime and never exits; _active only gates
    // whether it fetches. Spawning a thread per onOpen() leaks workers when
    // the user cycles apps faster than the old worker can exit - and when
    // thread creation eventually fails, std::thread aborts the firmware
    // (C++ exceptions are disabled).
    bool _worker_started = false;      // UI task only
    std::atomic<bool> _active{false};  // app open -> worker may fetch
    std::atomic<bool> _fresh{false};
    std::atomic<bool> _error{false};
    std::atomic<int> _temp_x10{0};     // 23.4 C -> 234 (atomics hold ints, not floats)
    std::atomic<int> _hum{0};
    std::atomic<int> _code{-1};        // WMO weather code
};
