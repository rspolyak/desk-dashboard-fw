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
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t display_init(int& width, int& height);

/**
 * @brief Draws a rectangle of pixels; returns once the pixels have been taken.
 * @note Call from the task that called display_init().
 * @param[in] x Left edge of the rectangle.
 * @param[in] y Top edge of the rectangle.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] pixels width * height RGB565 values, high byte first, row-major; read only during the call.
 */
void display_draw(int x, int y, int width, int height, const std::uint16_t* pixels);

/**
 * @brief Fades the display backlight to a new brightness over about one second.
 * @param[in] percent Brightness from 0 (off) to 100 (full).
 */
void display_backlight(int percent);

/**
 * @brief Sets up the indoor temperature and humidity sensor.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t climate_init();

/**
 * @brief Measures the indoor temperature and relative humidity.
 * @param[out] temperature_c Temperature in degrees Celsius.
 * @param[out] humidity_percent Relative humidity in percent, 0 to 100.
 * @return ESP_OK, or the error of the measurement.
 */
esp_err_t climate_read(float& temperature_c, float& humidity_percent);

/**
 * @brief Sets up the ambient light sensor.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t light_init();

/**
 * @brief Measures the ambient light.
 * @param[out] lux Approximate illuminance in lux.
 * @return ESP_OK, or the error of the measurement.
 */
esp_err_t light_read(float& lux);
} // namespace bsp
