/**
 * @file lvgl_port.cpp
 * @brief See lvgl_port.h.
 */
#include "lvgl_port.h"
#include "st7789.h"
#include "../hal_config.h"
#include "../touch/touch.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>

namespace lvgl_port {

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;
    // LVGL renders little-endian RGB565; the ST7789 wants big-endian. Swap in
    // place (the draw buffer is re-rendered each frame, so mutating is fine).
    uint16_t* p = reinterpret_cast<uint16_t*>(px_map);
    const int count = w * h;
    for (int i = 0; i < count; ++i) {
        const uint16_t v = p[i];
        p[i] = (uint16_t)((v >> 8) | (v << 8));
    }
    st7789::blit(area->x1, area->y1, area->x2, area->y2, p);
    lv_display_flush_ready(disp);
}

static void tick_timer(void* /*arg*/) { lv_tick_inc(10); }

// LVGL pointer indev: a touch on the top pad reports a press at screen
// center. There is no positional touch on this device — apps see taps as
// LV_EVENT_CLICKED at (120,120) only.
static void touch_read_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data)
{
    if (touch::pressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = HAL_SCREEN_WIDTH / 2;
        data->point.y = HAL_SCREEN_HEIGHT / 2;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void init()
{
    ESP_LOGI("lvgl", "init");
    lv_init();

    auto display = lv_display_create(HAL_SCREEN_WIDTH, HAL_SCREEN_HEIGHT);
    lv_display_set_flush_cb(display, flush_cb);

    // Partial double-buffer (60 rows each ~= 28 KB): fewer flush/set_window
    // overheads per frame than a small buffer, while staying far cheaper on
    // RAM than a 115 KB full-frame buffer (headroom for WiFi/BLE, no PSRAM).
    constexpr int BUF_LINES = 60;
    const size_t buf_bytes = (size_t)HAL_SCREEN_WIDTH * BUF_LINES * sizeof(uint16_t);
    void* buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    void* buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    assert(buf1 && buf2);
    lv_display_set_buffers(display, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, display);

    const esp_timer_create_args_t timer_args = {.callback = &tick_timer,
                                                .name = "lvgl_tick"};
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 10 * 1000));
}

} // namespace lvgl_port
