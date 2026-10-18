#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @file
 * @brief Board support for the desk dashboard PCB: its devices and their pins.
 */
namespace bsp {
/**
 * @brief Brings up the board's devices.
 * @return ESP_OK, or the error of the device that failed.
 */
esp_err_t init();

/**
 * @brief Prepares the display and reports its size.
 * @param[out] width Display width in pixels.
 * @param[out] height Display height in pixels.
 * @return ESP_OK, or ESP_ERR_NOT_SUPPORTED while the board has no display driver.
 */
esp_err_t display_init(int& width, int& height);

/**
 * @brief Draws a rectangle of pixels; returns once the pixels have been taken.
 * @param[in] x Left edge of the rectangle.
 * @param[in] y Top edge of the rectangle.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] pixels width * height native-endian RGB565 values, row-major; read only during the call.
 */
void display_draw(int x, int y, int width, int height, const std::uint16_t* pixels);

/**
 * @brief Switches the display backlight on or off.
 * @param[in] on true to switch the backlight on.
 */
void display_backlight(bool on);
} // namespace bsp
