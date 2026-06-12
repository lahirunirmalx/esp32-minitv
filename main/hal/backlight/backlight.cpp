/**
 * @file backlight.cpp
 * @brief See backlight.h. Pure ESP-IDF LEDC; both lines are ACTIVE-LOW, so
 *        full brightness = small duty (output mostly low).
 */
#include "backlight.h"
#include "../hal_config.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>

namespace {
constexpr ledc_timer_t   kTimer   = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
constexpr ledc_mode_t    kMode    = LEDC_LOW_SPEED_MODE;
constexpr int            kFreqHz  = 5000;
constexpr int            kResBits = 8;
constexpr int            kDutyMax = (1 << kResBits) - 1;

bool s_on = false;
}

namespace backlight {

static void apply_level(uint8_t pct)
{
    if (pct > 100) pct = 100;
    // Active-low: 100 % brightness -> duty 0.
    const uint32_t duty = (uint32_t)kDutyMax * (100 - pct) / 100;
    ledc_set_duty(kMode, kChannel, duty);
    ledc_update_duty(kMode, kChannel);
}

void init()
{
    // Coming back from deep sleep the pins are still held — release them
    // before reconfiguring (no-ops on a cold boot).
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)HAL_PIN_BACKLIGHT);
    gpio_hold_dis((gpio_num_t)HAL_PIN_PANEL_VDD);

    // Panel VDD enable (active-low) -> power the panel and keep it on.
    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << HAL_PIN_PANEL_VDD);
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level((gpio_num_t)HAL_PIN_PANEL_VDD, 0);

    ledc_timer_config_t timer = {};
    timer.speed_mode = kMode;
    timer.timer_num = kTimer;
    timer.duty_resolution = (ledc_timer_bit_t)kResBits;
    timer.freq_hz = kFreqHz;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {};
    ch.gpio_num = HAL_PIN_BACKLIGHT;
    ch.speed_mode = kMode;
    ch.channel = kChannel;
    ch.timer_sel = kTimer;
    ch.duty = kDutyMax; // active-low: full duty = dark
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    s_on = false;
    apply_level(0);
    ESP_LOGI("backlight", "init (BL=GPIO%d VDD=GPIO%d), default off",
             HAL_PIN_BACKLIGHT, HAL_PIN_PANEL_VDD);
}

void on()  { s_on = true;  apply_level(100); }
void off() { s_on = false; apply_level(0); }
void set_level(uint8_t pct) { s_on = pct > 0; apply_level(pct); }
bool is_on() { return s_on; }

void prepare_sleep()
{
    s_on = false;
    // Stop PWM with the line idling HIGH (active-low -> backlight off), then
    // power the panel down and freeze both pins for the duration of sleep.
    ledc_stop(kMode, kChannel, 1);
    gpio_set_level((gpio_num_t)HAL_PIN_PANEL_VDD, 1);
    gpio_hold_en((gpio_num_t)HAL_PIN_BACKLIGHT);
    gpio_hold_en((gpio_num_t)HAL_PIN_PANEL_VDD);
    gpio_deep_sleep_hold_en();
}

} // namespace backlight
