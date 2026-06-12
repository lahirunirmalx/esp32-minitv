/**
 * @file lvgl_port.h
 * @brief LVGL 9 bring-up for the Mini TV: display + flush callback, partial
 *        double-buffer, tick timer, and the touch-pad pointer indev.
 *
 * THREADING: LVGL is not thread-safe. Every lv_* call (including everything
 * apps do) must happen on the UI task (core 1). Worker threads hand results
 * back via queues/atomics; see app_demo for the pattern.
 */
#pragma once

namespace lvgl_port {

// lv_init + display + buffers + indev + 10 ms tick timer.
// Call after st7789::chip_init().
void init();

} // namespace lvgl_port
