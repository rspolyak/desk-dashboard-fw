#include "app/app.hpp"
#include "bsp/bsp.hpp"
#include "dashboard.hpp"
#include "lvgl_port.hpp"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
 * @brief Reads the indoor sensor every few seconds and redraws the changed tiles; never returns.
 * @param[in] labels The INDOOR and HUMIDITY value labels.
 */
[[noreturn]] void refresh_climate_forever(const dashboard::ClimateLabels& labels)
{
    constexpr std::uint32_t refresh_period_ms = 5000;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(refresh_period_ms));
        show_climate(labels);
        lv_refr_now(nullptr);
    }
}
} // namespace

/**
 * @brief Brings up the board, draws the dashboard and keeps the indoor values up to date.
 * @return Only if setup fails, with its error, or with ESP_OK if the sensor is missing and nothing needs updating.
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
    bsp::display_backlight(true);

    if (climate_ready) {
        refresh_climate_forever(climate_labels);
    }
    return ESP_OK;
}
} // namespace app
