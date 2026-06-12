/**
 * @file st7789.h
 * @brief ST7789 240x240 SPI panel backend (pins in hal_config.h).
 *
 * Ported from the proven phoebe/esp32_minitv driver: pure ESP-IDF
 * spi_master, DC line driven from the SPI pre-transfer callback.
 */
#pragma once
#include <cstdint>

namespace st7789 {

// Bring up the SPI bus and run the panel init sequence (SWRESET path — this
// board has no reset pin wired). Must run after the panel VDD is enabled.
void chip_init();

// Push a rectangle of big-endian RGB565 pixels to the panel.
void blit(int x1, int y1, int x2, int y2, const uint16_t* data);

} // namespace st7789
