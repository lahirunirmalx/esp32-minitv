/**
 * @file touch.cpp
 * @brief See touch.h. Ported from the proven phoebe T9 driver (legacy
 *        ESP32 touch_pad v1 API): IIR filter @10 Hz, 16-sample baseline,
 *        +/- baseline/8 delta band counts as a touch.
 */
#include "touch.h"
#include "../hal_config.h"
#include <driver/touch_pad.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

namespace touch {

static const char* TAG = "touch";

static uint16_t s_baseline = 0;
static uint16_t s_delta = 1;        // +/- band that counts as a touch
static bool s_pressed = false;

// Gesture state
static bool s_press_active = false; // physical hold in progress
static uint32_t s_press_start = 0;
static bool s_long_fired = false;   // long-press already reported this hold
static volatile bool s_long_pending = false;

static uint16_t read_raw()
{
    uint16_t v = 0;
    touch_pad_read_filtered(HAL_TOUCH_PAD_NUM, &v);
    return v;
}

void init()
{
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(HAL_TOUCH_PAD_NUM, 0);
    touch_pad_filter_start(10);

    vTaskDelay(pdMS_TO_TICKS(200)); // let the IIR filter settle (untouched)
    uint32_t sum = 0;
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        sum += read_raw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_baseline = (uint16_t)(sum / n);
    s_delta = s_baseline / 8; // ~12% deviation
    if (s_delta < 1) s_delta = 1;
    ESP_LOGI(TAG, "T9/GPIO32 baseline=%u delta=%u", s_baseline, s_delta);
}

void tick(uint32_t now_ms)
{
    const uint16_t v = read_raw();
    const bool down = (v + s_delta < s_baseline) || (v > s_baseline + s_delta);

    if (down && !s_press_active) {
        s_press_active = true;
        s_press_start = now_ms;
        s_long_fired = false;
    } else if (!down && s_press_active) {
        s_press_active = false;
    }

    // Long-press: fire once when the hold passes the threshold.
    if (s_press_active && !s_long_fired &&
        (now_ms - s_press_start >= HAL_TOUCH_LONGPRESS_MS)) {
        s_long_fired = true;
        s_long_pending = true;
    }

    // Suppress the "pressed" report once a long-press has fired, so the
    // eventual release doesn't also register as a tap.
    s_pressed = s_press_active && !s_long_fired;
}

bool pressed() { return s_pressed; }

bool take_long_press()
{
    if (!s_long_pending) return false;
    s_long_pending = false;
    return true;
}

void prepare_wakeup()
{
    // ESP32 v1 touch: a touch LOWERS the raw reading; the wake interrupt
    // fires when the reading drops below the threshold.
    const uint16_t thresh = (s_baseline > s_delta) ? (uint16_t)(s_baseline - s_delta) : 1;
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER); // hardware-timed sampling in sleep
    touch_pad_set_thresh(HAL_TOUCH_PAD_NUM, thresh);
    ESP_LOGI(TAG, "armed for deep-sleep wake (thresh=%u)", thresh);
}

} // namespace touch
