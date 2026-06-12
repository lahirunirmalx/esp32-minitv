/**
 * @file touch.h
 * @brief Capacitive touch pad on TOP of the TV cube (T9 / GPIO32).
 *
 * The display is NOT a touchscreen. This single pad is the device's only
 * input; it yields gestures, not positions:
 *   - tap        (release before HAL_TOUCH_TAP_MAX_MS)
 *   - hold       (longer press, decided on release by the caller)
 *   - long-press (>= HAL_TOUCH_LONGPRESS_MS, fired once while still held)
 *
 * tick() must be called regularly (every ~5 ms) from the UI task.
 */
#pragma once
#include <cstdint>

namespace touch {

// Init the touch pad, IIR filter, and baseline calibration. Nothing may be
// touching the pad during the ~400 ms calibration window.
void init();

// Sample + update gesture state. Call from the UI task loop.
void tick(uint32_t now_ms);

// Physical press state, suppressed after a long-press fires so the eventual
// release doesn't double as a tap. Feeds the LVGL pointer indev.
bool pressed();

// One-shot: true exactly once per >=3 s hold.
bool take_long_press();

// Arm the pad as a deep-sleep wake source: switches the touch FSM to its
// hardware timer and sets the wake threshold from the boot calibration.
// Call right before esp_deep_sleep_start().
void prepare_wakeup();

} // namespace touch
