#include "bsp/bsp.hpp"

/**
 * @file
 * @brief Board support for the desk dashboard PCB with ESP32-S3 (github.com/rspolyak/desk-dashboard-hw).
 */
namespace bsp {
/**
 * @brief Brings up the board's devices.
 * @return ESP_OK.
 */
esp_err_t init() { return ESP_OK; }

/**
 * @brief Reports that this board has no display driver.
 * @param[out] width Left unchanged.
 * @param[out] height Left unchanged.
 * @return ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t display_init([[maybe_unused]] int& width, [[maybe_unused]] int& height)
{
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief Does nothing; this board has no display driver.
 * @param[in] x Unused.
 * @param[in] y Unused.
 * @param[in] width Unused.
 * @param[in] height Unused.
 * @param[in] pixels Unused.
 */
void display_draw([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int width,
                  [[maybe_unused]] int height, [[maybe_unused]] const std::uint16_t* pixels)
{
}

/**
 * @brief Does nothing; this board has no display driver.
 * @param[in] on Unused.
 */
void display_backlight([[maybe_unused]] bool on) {}
} // namespace bsp
