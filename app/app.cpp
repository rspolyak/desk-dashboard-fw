#include "app/app.hpp"
#include "bsp/bsp.hpp"
#include "dashboard.hpp"
#include "lvgl_port.hpp"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace app {
namespace {
constexpr const char* log_tag = "dashboard";

/**
 * @brief Reads the indoor sensor and shows the values in the tiles; on a failed read the tiles keep their values.
 * @param[in] labels The INDOOR and HUMIDITY value labels.
 */
void show_climate(const dashboard::ClimateLabels& labels)
{
    constexpr std::size_t tile_text_bytes = 16;

    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
    if (bsp::climate_read(temperature_c, humidity_percent) != ESP_OK) {
        return;
    }
    ESP_LOGI(log_tag, "indoor %.1f C, %.0f %% humidity", temperature_c, humidity_percent);

    char temperature_text[tile_text_bytes] = {0};
    char humidity_text[tile_text_bytes] = {0};
    std::snprintf(temperature_text, sizeof(temperature_text), "%.1f\xC2\xB0" "C", temperature_c);
    std::snprintf(humidity_text, sizeof(humidity_text), "%.0f %%", humidity_percent);
    lv_label_set_text(labels.temperature, temperature_text);
    lv_label_set_text(labels.humidity, humidity_text);
}

/**
 * @brief Picks the backlight brightness for an ambient light level; it rises with the logarithm of the light, as the eye sees it.
 * @param[in] lux Ambient light in lux.
 * @return Brightness in percent: dark_percent at dark_lux or less, 100 at bright_lux or more.
 */
int backlight_percent_for(float lux)
{
    constexpr float dark_lux = 10.0f;
    constexpr float bright_lux = 250.0f;
    constexpr float dark_percent = 10.0f;
    constexpr float bright_percent = 100.0f;

    const float limited_lux = std::clamp(lux, dark_lux, bright_lux);
    const float position = std::log(limited_lux / dark_lux) / std::log(bright_lux / dark_lux);
    return static_cast<int>(std::lround(dark_percent + position * (bright_percent - dark_percent)));
}

/**
 * @brief Reads the ambient light and fades the backlight to match; keeps the brightness if the read fails.
 * @return ESP_OK, or the error of the light read, which the board has already logged.
 */
esp_err_t adjust_backlight()
{
    float lux = 0.0f;
    const esp_err_t status = bsp::light_read(lux);
    if (status != ESP_OK) {
        return status;
    }

    const int percent = backlight_percent_for(lux);
    ESP_LOGI(log_tag, "light %.0f lux, backlight %d %%", lux, percent);
    bsp::display_backlight(percent);
    return ESP_OK;
}

/**
 * @brief Every few seconds reads the sensors that were set up, redraws the changed tiles and adjusts the backlight; never returns.
 * @param[in] labels The INDOOR and HUMIDITY value labels.
 * @param[in] climate_ready true if the temperature and humidity sensor was set up.
 * @param[in] light_ready true if the ambient light sensor was set up.
 */
[[noreturn]] void refresh_forever(const dashboard::ClimateLabels& labels, bool climate_ready, bool light_ready)
{
    constexpr std::uint32_t refresh_period_ms = 5000;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(refresh_period_ms));

        if (climate_ready) {
            show_climate(labels);
            lv_refr_now(nullptr);
        }

        if (light_ready) {
            (void)adjust_backlight();
        }
    }
}
} // namespace

/**
 * @brief Brings up the board, draws the dashboard and keeps the indoor values up to date.
 * @return Only if the board or the display cannot be set up, with the error of that step.
 */
esp_err_t start()
{
    esp_err_t status = bsp::init();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "board init failed: %s", esp_err_to_name(status));
        return status;
    }
    ESP_LOGI(log_tag, "starting");

    const bool climate_ready = bsp::climate_init() == ESP_OK;
    const bool light_ready = bsp::light_init() == ESP_OK;

    status = lvgl_port::start();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display setup failed: %s", esp_err_to_name(status));
        return status;
    }

    const dashboard::ClimateLabels climate_labels = dashboard::create();
    if (climate_ready) {
        show_climate(climate_labels);
    }
    lv_refr_now(nullptr);

    constexpr int full_brightness_percent = 100;
    bool backlight_set = false;
    if (light_ready) {
        backlight_set = adjust_backlight() == ESP_OK;
    }
    if (!backlight_set) {
        bsp::display_backlight(full_brightness_percent);
    }

    refresh_forever(climate_labels, climate_ready, light_ready);
}
} // namespace app
