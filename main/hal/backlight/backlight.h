/**
 * @file backlight.h
 * @brief Active-low backlight PWM (GPIO19) + panel VDD enable (GPIO21).
 */
#pragma once
#include <cstdint>

namespace backlight {

// Power the panel (VDD enable low) and set up LEDC PWM. Starts dark.
void init();

void on();                       // 100 %
void off();                      // 0 %
void set_level(uint8_t pct);     // 0..100
bool is_on();

// Park the panel for deep sleep: backlight + panel VDD both OFF, with GPIO
// holds so the active-low lines can't float low (= panel lighting up) while
// the chip sleeps. init() releases the holds on the next boot.
void prepare_sleep();

} // namespace backlight
