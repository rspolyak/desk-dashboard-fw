#include "bsp/bsp.hpp"
#include "driver/gpio.h"
#include "esp_log.h"

/**
 * @file
 * @brief Board support for the desk dashboard PCB with ESP32-S3 (github.com/rspolyak/desk-dashboard-hw).
 */
namespace bsp {
namespace {
constexpr const char* log_tag = "bsp";
constexpr gpio_num_t backlight_pin = GPIO_NUM_47;
constexpr std::uint32_t backlight_on_level = 1;
constexpr std::uint32_t backlight_off_level = 0;
} // namespace

/**
 * @brief Brings up the board's devices; the backlight starts off.
 * @return ESP_OK, or the error of the pin setup that failed.
 */
esp_err_t init()
{
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << backlight_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t status = gpio_config(&backlight_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight pin setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = gpio_set_level(backlight_pin, backlight_off_level);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "switching the backlight off failed: %s", esp_err_to_name(status));
        return status;
    }
    return ESP_OK;
}

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
 * @brief Switches the display backlight on or off.
 * @param[in] on true to switch the backlight on.
 */
void display_backlight(bool on)
{
    const std::uint32_t level = on ? backlight_on_level : backlight_off_level;
    const esp_err_t status = gpio_set_level(backlight_pin, level);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "switching the backlight failed: %s", esp_err_to_name(status));
    }
}
} // namespace bsp
