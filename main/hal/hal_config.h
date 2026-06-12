/**
 * @file hal_config.h
 * @brief Pin map + screen geometry for the Freenove ESP32 Mini TV (FNK0112).
 *
 * Classic ESP32 ("ESP-32S" module marking, 4 MB flash, no PSRAM).
 * 240x240 ST7789 over HSPI, capacitive touch pad T9 on GPIO32 (the touch
 * area on TOP of the cube — the display itself is NOT a touchscreen),
 * backlight + panel-VDD both active-low. Pins confirmed on real hardware.
 *
 * Every pin used by the firmware lives in this file.
 */
#pragma once

// ---- Screen ---------------------------------------------------------------
#define HAL_SCREEN_WIDTH  240
#define HAL_SCREEN_HEIGHT 240

// ---- ST7789 display (HSPI) ------------------------------------------------
#define HAL_PIN_LCD_SCLK 14
#define HAL_PIN_LCD_MOSI 13
#define HAL_PIN_LCD_DC    2
#define HAL_PIN_LCD_CS   15
#define HAL_PIN_LCD_RST  (-1)   // no reset pin wired -> software reset only
#define HAL_LCD_SPI_HOST  SPI2_HOST
// 80 MHz: SCLK 14 / MOSI 13 are the SPI2 (HSPI) IO_MUX pins, so the driver can
// clock at the ESP32 max for the fastest LVGL flush. If the panel ever shows
// garbled/torn output, step down to 40 MHz.
#define HAL_LCD_SPI_HZ   (80 * 1000 * 1000)

// ---- Backlight + panel power (both ACTIVE-LOW) ----------------------------
#define HAL_PIN_BACKLIGHT 19    // LEDC PWM, active-low (drive low = on)
#define HAL_PIN_PANEL_VDD 21    // active-low enable; must be low to power the panel

// ---- Capacitive touch (pad on top of the cube) ----------------------------
#define HAL_TOUCH_PAD_NUM  TOUCH_PAD_NUM9   // T9 == GPIO32
#define HAL_TOUCH_LONGPRESS_MS 3000         // hold this long -> open captive portal
#define HAL_TOUCH_TAP_MAX_MS   600          // release before this = tap
#define HAL_DISPLAY_SLEEP_MS   60000        // backlight off after this much idle
#define HAL_DEEP_SLEEP_MS      300000       // deep sleep after this much idle (5 min);
                                            // touching the pad wakes (= reboots) the device
